//go:build windows

package grpc_server

import (
	"os"
	"path/filepath"
	"strings"
	"syscall"
	"testing"
	"time"

	"grpc_server/gen"
)

// These scenarios reproduce what stranded Windows users: a scanner (antivirus) holding the freshly downloaded
// file open, so renaming or deleting it fails with ERROR_SHARING_VIOLATION. They run only on windows-latest
// (job "Windows update E2E"); every path lives under t.TempDir().

// openShared opens path the way a scanner does: it shares read and write but not delete, so rename and delete
// fail while it is held, yet reading still works.
func openShared(path string) (syscall.Handle, error) {
	p, err := syscall.UTF16PtrFromString(path)
	if err != nil {
		return 0, err
	}
	return syscall.CreateFile(p, syscall.GENERIC_READ,
		syscall.FILE_SHARE_READ|syscall.FILE_SHARE_WRITE, nil,
		syscall.OPEN_EXISTING, syscall.FILE_ATTRIBUTE_NORMAL, 0)
}

// holdOpen keeps path open for d. done is closed once the handle is released.
func holdOpen(t *testing.T, path string, d time.Duration) (<-chan struct{}, error) {
	t.Helper()
	h, err := openShared(path)
	if err != nil {
		return nil, err
	}
	done := make(chan struct{})
	go func() {
		time.Sleep(d)
		_ = syscall.CloseHandle(h)
		close(done)
	}()
	t.Cleanup(func() { <-done })
	return done, nil
}

func firstPart(dir string) string {
	entries, err := os.ReadDir(dir)
	if err != nil {
		return ""
	}
	for _, e := range entries {
		n := e.Name()
		if strings.HasPrefix(n, "update-package.zip") && strings.HasSuffix(n, ".part") {
			return filepath.Join(dir, n)
		}
	}
	return ""
}

// holdFirstPart waits (up to within) for the first temp download to appear in dir and holds it open for d.
// held receives its path; done is closed when it was released or when it gave up.
func holdFirstPart(t *testing.T, dir string, d, within time.Duration) (held <-chan string, done <-chan struct{}) {
	t.Helper()
	heldCh := make(chan string, 1)
	doneCh := make(chan struct{})
	t.Cleanup(func() { <-doneCh })
	go func() {
		defer close(doneCh)
		deadline := time.Now().Add(within)
		for time.Now().Before(deadline) {
			p := firstPart(dir)
			if p == "" {
				time.Sleep(5 * time.Millisecond)
				continue
			}
			h, err := openShared(p)
			if err != nil {
				if _, statErr := os.Stat(p); statErr != nil {
					continue // gone: look for the next one
				}
				time.Sleep(5 * time.Millisecond)
				continue
			}
			heldCh <- p
			time.Sleep(d)
			_ = syscall.CloseHandle(h)
			return
		}
	}()
	return heldCh, doneCh
}

func checkAndDownload(t *testing.T) *gen.UpdateResp {
	t.Helper()
	if check := callUpdate(t, gen.UpdateAction_Check); check.Error != "" {
		t.Fatalf("Check failed: %s", check.Error)
	}
	return callUpdate(t, gen.UpdateAction_Download)
}

func requireStaged(t *testing.T, in *e2eInstall) {
	t.Helper()
	got := sha256Hex([]byte(readText(t, filepath.Join(in.root, "update-package.zip"))))
	if got != sha256Hex(in.pkg) {
		t.Fatalf("staged package digest %s, want %s", got, sha256Hex(in.pkg))
	}
}

func TestUpdateE2EWindowsScannerHoldsTheDownload(t *testing.T) {
	in := stageE2E(t, false)
	in.srv.setSlowAsset(400 * time.Millisecond)
	held, done := holdFirstPart(t, in.root, 3*time.Second, 10*time.Second)

	if check := callUpdate(t, gen.UpdateAction_Check); check.Error != "" {
		t.Fatalf("Check failed: %s", check.Error)
	}
	start := time.Now()
	dl := callUpdate(t, gen.UpdateAction_Download)
	elapsed := time.Since(start)
	if dl.Error != "" {
		t.Fatalf("Download failed while a scanner held the file: %s", dl.Error)
	}
	select {
	case <-held:
	default:
		t.Fatal("the scanner stand-in never held the temp download")
	}
	if elapsed < 2500*time.Millisecond {
		t.Fatalf("Download took %v: the hold (3 s) must outlive the old 2 s retry budget", elapsed)
	}
	requireStaged(t, in)
	<-done
	if parts := partFiles(t, in.root); len(parts) != 0 {
		t.Fatalf("leftover temp downloads: %v", parts)
	}
}

func TestUpdateE2EWindowsLockedLegacyPartDoesNotBlock(t *testing.T) {
	in := stageE2E(t, false)
	legacy := filepath.Join(in.root, "update-package.zip.part")
	if err := os.WriteFile(legacy, []byte("stale"), 0o644); err != nil {
		t.Fatal(err)
	}
	done, err := holdOpen(t, legacy, 6*time.Second)
	if err != nil {
		t.Fatal(err)
	}

	start := time.Now()
	dl := checkAndDownload(t)
	elapsed := time.Since(start)
	if dl.Error != "" {
		t.Fatalf("Download failed next to a locked legacy .part: %s", dl.Error)
	}
	if elapsed >= 5*time.Second {
		t.Fatalf("Download took %v: it must never wait for the locked legacy .part", elapsed)
	}
	requireStaged(t, in)
	if _, err := os.Stat(legacy); err != nil {
		t.Fatalf("the locked legacy .part should still exist while held: %v", err)
	}

	<-done
	if dl := checkAndDownload(t); dl.Error != "" {
		t.Fatalf("second Download failed: %s", dl.Error)
	}
	if _, err := os.Stat(legacy); err == nil {
		t.Fatal("the legacy .part should be cleaned once it is no longer held")
	}
}

func TestUpdateE2EWindowsCopiesWhenRenameStaysBlocked(t *testing.T) {
	in := stageE2E(t, false)
	waits := stubRetrySleep(t)
	in.srv.setSlowAsset(400 * time.Millisecond)
	held, done := holdFirstPart(t, in.root, 3*time.Second, 10*time.Second)

	dl := checkAndDownload(t)
	if dl.Error != "" {
		t.Fatalf("Download should fall back to copying, got: %s", dl.Error)
	}
	select {
	case <-held:
	default:
		t.Fatal("the scanner stand-in never held the temp download")
	}
	if len(*waits) != renameAttempts-1 {
		t.Fatalf("rename waits = %d, want %d (every rename attempt must fail first)", len(*waits), renameAttempts-1)
	}
	requireStaged(t, in)

	<-done
	if dl := checkAndDownload(t); dl.Error != "" {
		t.Fatalf("second Download failed: %s", dl.Error)
	}
	if parts := partFiles(t, in.root); len(parts) != 0 {
		t.Fatalf("leftover temp downloads after the next attempt: %v", parts)
	}
}

func TestUpdateE2EWindowsAppliesWhileTheOldCoreStillExits(t *testing.T) {
	in := stageE2E(t, true)
	if dl := checkAndDownload(t); dl.Error != "" {
		t.Fatalf("Download failed: %s", dl.Error)
	}
	t.Chdir(t.TempDir())

	done, err := holdOpen(t, filepath.Join(in.root, "proxor_core.exe"), 3*time.Second)
	if err != nil {
		t.Fatal(err)
	}
	runUpdater(t, in.root, in.updater)
	waitForApplied(t, in.root, "new")

	if got := readText(t, filepath.Join(in.root, "proxor_core.exe")); got != "new-core" {
		t.Fatalf("proxor_core.exe = %q, want new-core", got)
	}
	if got := readText(t, filepath.Join(in.root, "config", "settings.json")); got != "user" {
		t.Fatalf("config/settings.json = %q, the user config must survive", got)
	}
	if _, err := os.Stat(filepath.Join(in.root, "update-error.txt")); err == nil {
		t.Fatalf("update-error.txt should not exist: %s", readText(t, filepath.Join(in.root, "update-error.txt")))
	}
	<-done
}

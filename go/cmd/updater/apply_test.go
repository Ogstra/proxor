package main

import (
	"archive/zip"
	"errors"
	"os"
	"path/filepath"
	"strings"
	"testing"
	"time"
)

func setHostGOOS(t *testing.T, goos string) {
	t.Helper()
	old := hostGOOS
	hostGOOS = goos
	t.Cleanup(func() { hostGOOS = old })
}

func writeFile(t *testing.T, path, content string) {
	t.Helper()
	if err := os.MkdirAll(filepath.Dir(path), 0755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(path, []byte(content), 0644); err != nil {
		t.Fatal(err)
	}
}

func readFile(t *testing.T, path string) string {
	t.Helper()
	b, err := os.ReadFile(path)
	if err != nil {
		t.Fatalf("read %s: %v", path, err)
	}
	return string(b)
}

func writeZip(t *testing.T, path string, files map[string]string) {
	t.Helper()
	f, err := os.Create(path)
	if err != nil {
		t.Fatal(err)
	}
	defer f.Close()
	zw := zip.NewWriter(f)
	for name, content := range files {
		w, err := zw.Create(name)
		if err != nil {
			t.Fatal(err)
		}
		if _, err := w.Write([]byte(content)); err != nil {
			t.Fatal(err)
		}
	}
	if err := zw.Close(); err != nil {
		t.Fatal(err)
	}
}

// oldInstall chdirs into a fresh temp install that starts the old version.
func oldInstall(t *testing.T) {
	t.Helper()
	t.Chdir(t.TempDir())
	writeFile(t, "proxor.exe", "old")
	writeFile(t, "libcronet.dll", "old")
	writeFile(t, "config/runtime/app.txt", "old-runtime")
	writeFile(t, "platforms/qwindows.txt", "old-platform")
	writeFile(t, "config/settings.json", "user")
}

func assertOldInstall(t *testing.T) {
	t.Helper()
	want := map[string]string{
		"proxor.exe":             "old",
		"libcronet.dll":          "old",
		"config/runtime/app.txt": "old-runtime",
		"platforms/qwindows.txt": "old-platform",
		"config/settings.json":   "user",
	}
	for p, c := range want {
		if got := readFile(t, p); got != c {
			t.Errorf("%s = %q, want %q", p, got, c)
		}
	}
	if Exist("update-package") {
		t.Errorf("./update-package must not exist")
	}
}

func TestReplaceAttemptsPolicy(t *testing.T) {
	if got := replaceAttempts("windows"); got != 20 {
		t.Errorf("windows attempts = %d, want 20", got)
	}
	if got := replaceAttempts("linux"); got != 1 {
		t.Errorf("linux attempts = %d, want 1", got)
	}
	if got := replaceAttempts("darwin"); got != 1 {
		t.Errorf("darwin attempts = %d, want 1", got)
	}
	if replaceRetryDelay != 250*time.Millisecond {
		t.Errorf("replaceRetryDelay = %v", replaceRetryDelay)
	}
	if time.Duration(replaceAttemptsWindows)*replaceRetryDelay != 5*time.Second {
		t.Errorf("total wait is not 5 s")
	}
}

func TestMoveReplacingRetriesAHeldFileOnWindows(t *testing.T) {
	setHostGOOS(t, "windows")
	dir := t.TempDir()
	src := filepath.Join(dir, "src.txt")
	dst := filepath.Join(dir, "dst.txt")
	writeFile(t, src, "new")
	writeFile(t, dst, "old")

	oldRemove, oldSleep := replaceRemoveAll, replaceSleep
	t.Cleanup(func() { replaceRemoveAll, replaceSleep = oldRemove, oldSleep })
	failures := 0
	replaceRemoveAll = func(path string) error {
		if path == dst && failures < 2 {
			failures++
			return errors.New("the process cannot access the file because it is being used by another process")
		}
		return os.RemoveAll(path)
	}
	var waits []time.Duration
	replaceSleep = func(d time.Duration) { waits = append(waits, d) }

	if err := moveReplacing(src, dst); err != nil {
		t.Fatalf("moveReplacing: %v", err)
	}
	if got := readFile(t, dst); got != "new" {
		t.Errorf("dst = %q, want new", got)
	}
	if len(waits) != 2 || waits[0] != 250*time.Millisecond || waits[1] != 250*time.Millisecond {
		t.Errorf("waits = %v, want 2 x 250ms", waits)
	}
}

func TestMoveReplacingTriesOnceElsewhere(t *testing.T) {
	setHostGOOS(t, "linux")
	dir := t.TempDir()
	src := filepath.Join(dir, "src.txt")
	dst := filepath.Join(dir, "dst.txt")
	writeFile(t, src, "new")
	writeFile(t, dst, "old")

	oldRemove, oldSleep := replaceRemoveAll, replaceSleep
	t.Cleanup(func() { replaceRemoveAll, replaceSleep = oldRemove, oldSleep })
	busy := errors.New("busy")
	replaceRemoveAll = func(path string) error { return busy }
	waits := 0
	replaceSleep = func(time.Duration) { waits++ }

	err := moveReplacing(src, dst)
	if !errors.Is(err, busy) {
		t.Fatalf("err = %v, want it to wrap busy", err)
	}
	if waits != 0 {
		t.Errorf("waits = %d, want 0", waits)
	}
}

func TestApplyUpdateInstallsAProxorRootedZip(t *testing.T) {
	setHostGOOS(t, "windows")
	t.Chdir(t.TempDir())
	writeFile(t, "proxor.exe", "old")
	writeFile(t, "version.txt", "old")
	writeFile(t, "libcronet.dll", "old")
	writeFile(t, "config/settings.json", "user")
	writeZip(t, "update-package.zip", map[string]string{
		"proxor/proxor.exe":             "new",
		"proxor/version.txt":            "new",
		"proxor/config/runtime/app.txt": "rt",
	})

	moved, err := applyUpdate()
	if err != nil || !moved {
		t.Fatalf("applyUpdate = (%v, %v), want (true, nil)", moved, err)
	}
	if got := readFile(t, "proxor.exe"); got != "new" {
		t.Errorf("proxor.exe = %q", got)
	}
	if got := readFile(t, "version.txt"); got != "new" {
		t.Errorf("version.txt = %q", got)
	}
	if got := readFile(t, "config/runtime/app.txt"); got != "rt" {
		t.Errorf("app.txt = %q", got)
	}
	if got := readFile(t, "config/settings.json"); got != "user" {
		t.Errorf("settings.json = %q, want user", got)
	}
	if Exist("libcronet.dll") {
		t.Errorf("stale dll should be removed")
	}
	if Exist("update-package.zip") || Exist("update-package") {
		t.Errorf("package and scratch dir must be gone")
	}
}

func TestApplyUpdateWithoutAPackageChangesNothing(t *testing.T) {
	setHostGOOS(t, "windows")
	oldInstall(t)
	moved, err := applyUpdate()
	if moved || err == nil || !strings.Contains(err.Error(), "no update package was found") {
		t.Fatalf("applyUpdate = (%v, %v)", moved, err)
	}
	assertOldInstall(t)
}

func TestApplyUpdateWithACorruptPackageChangesNothing(t *testing.T) {
	setHostGOOS(t, "windows")
	oldInstall(t)
	writeFile(t, "update-package.zip", "not a zip")
	moved, err := applyUpdate()
	if moved || err == nil {
		t.Fatalf("applyUpdate = (%v, %v)", moved, err)
	}
	assertOldInstall(t)
}

func TestApplyUpdateWithAnEmptyPackageChangesNothing(t *testing.T) {
	setHostGOOS(t, "windows")
	oldInstall(t)
	writeFile(t, "update-package.zip", "")
	moved, err := applyUpdate()
	if moved || err == nil {
		t.Fatalf("applyUpdate = (%v, %v)", moved, err)
	}
	assertOldInstall(t)
}

func TestApplyUpdateWithoutProxorExeChangesNothingOnWindows(t *testing.T) {
	setHostGOOS(t, "windows")
	oldInstall(t)
	writeZip(t, "update-package.zip", map[string]string{"proxor/readme.txt": "hi"})
	moved, err := applyUpdate()
	if moved || err == nil || !strings.Contains(err.Error(), "no proxor.exe") {
		t.Fatalf("applyUpdate = (%v, %v)", moved, err)
	}
	assertOldInstall(t)

	// The check is Windows-only: the same package proceeds elsewhere.
	setHostGOOS(t, "linux")
	moved, err = applyUpdate()
	if !moved || err != nil {
		t.Fatalf("linux applyUpdate = (%v, %v), want (true, nil)", moved, err)
	}
}

func TestApplyUpdateRemovesObsoletePathsOnlyAfterValidation(t *testing.T) {
	setHostGOOS(t, "windows")
	oldInstall(t)
	writeZip(t, "update-package.zip", map[string]string{
		"proxor/proxor.exe":             "new",
		"proxor/config/runtime/app.txt": "new-runtime",
	})
	moved, err := applyUpdate()
	if !moved || err != nil {
		t.Fatalf("applyUpdate = (%v, %v)", moved, err)
	}
	if Exist("platforms") {
		t.Errorf("obsolete ./platforms should be removed after a valid update")
	}
	if got := readFile(t, "config/runtime/app.txt"); got != "new-runtime" {
		t.Errorf("app.txt = %q", got)
	}
}

func TestPlanAfterFailure(t *testing.T) {
	cause := errors.New("boom")
	dir := `C:\Proxor`
	tests := []struct {
		name     string
		goos     string
		moved    bool
		headless bool
		dialog   bool
		relaunch bool
		errFile  bool
		exit     int
		contains []string
	}{
		{"windows untouched", "windows", false, false, true, true, true, 0, []string{"keeps working", releasesPageURL, "boom"}},
		{"windows untouched headless", "windows", false, true, false, true, true, 0, []string{"keeps working", releasesPageURL}},
		{"windows moved", "windows", true, false, true, false, true, 1, []string{dir, releasesPageURL, "boom"}},
	}
	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			p := planAfterFailure(tc.goos, tc.moved, cause, tc.headless, dir)
			if p.dialog != tc.dialog || p.relaunch != tc.relaunch || p.writeErrorFile != tc.errFile || p.exitCode != tc.exit {
				t.Errorf("plan = %+v", p)
			}
			for _, c := range tc.contains {
				if !strings.Contains(p.message, c) {
					t.Errorf("message %q lacks %q", p.message, c)
				}
			}
		})
	}
	for _, goos := range []string{"linux", "darwin"} {
		p := planAfterFailure(goos, false, cause, false, "/opt/proxor")
		if p.relaunch || p.dialog || p.writeErrorFile || p.exitCode != 1 || p.message != cause.Error() {
			t.Errorf("%s plan = %+v", goos, p)
		}
	}
}

func TestRunUpdateHeadlessFailureWritesErrorFileAndRelaunches(t *testing.T) {
	setHostGOOS(t, "windows")
	t.Setenv(headlessEnv, "1")
	t.Chdir(t.TempDir())
	if !runUpdate() {
		t.Fatal("runUpdate() = false, want true (relaunch the untouched install)")
	}
	msg := readFile(t, "update-error.txt")
	if !strings.Contains(msg, "Nothing was changed") || !strings.Contains(msg, releasesPageURL) {
		t.Errorf("update-error.txt = %q", msg)
	}
}

func TestRunUpdateSuccessRemovesAStaleErrorFile(t *testing.T) {
	setHostGOOS(t, "windows")
	t.Setenv(headlessEnv, "1")
	t.Chdir(t.TempDir())
	writeFile(t, "update-error.txt", "stale")
	writeZip(t, "update-package.zip", map[string]string{"proxor/proxor.exe": "new"})
	if !runUpdate() {
		t.Fatal("runUpdate() = false, want true")
	}
	if Exist("update-error.txt") {
		t.Errorf("stale update-error.txt should be removed")
	}
}

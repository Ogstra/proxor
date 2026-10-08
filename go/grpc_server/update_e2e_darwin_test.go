//go:build darwin

package grpc_server

import (
	"context"
	"grpc_server/gen"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"runtime"
	"strconv"
	"strings"
	"syscall"
	"testing"
	"time"
)

// macOS end-to-end update path: the candidate core downloads and verifies the zip through Update() against the
// local fake release server, then the candidate relauncher (packaging/macos/proxor-app-update.sh) swaps a
// THROWAWAY bundle (own bundle id, temp dirs) and relaunches it with the test-only exec seam. Proxor itself is
// never started. With PROXOR_MAC_UPDATE_E2E_REQUIRE=1 (the CI gate) missing tools fail instead of skipping.

func macTool(t *testing.T) string {
	t.Helper()
	pkgDir, err := os.Getwd()
	if err != nil {
		t.Fatal(err)
	}
	root := filepath.Clean(filepath.Join(pkgDir, "..", ".."))
	missing := ""
	if _, err := exec.LookPath("ditto"); err != nil {
		missing = "ditto"
	} else if _, err := exec.LookPath("codesign"); err != nil {
		missing = "codesign"
	} else if _, err := os.Stat("/usr/bin/cc"); err != nil {
		if _, err := exec.LookPath("xcrun"); err != nil {
			missing = "a C compiler (xcrun clang or /usr/bin/cc)"
		}
	}
	if missing != "" {
		if os.Getenv("PROXOR_MAC_UPDATE_E2E_REQUIRE") == "1" {
			t.Fatalf("PROXOR_MAC_UPDATE_E2E_REQUIRE=1 but %s is missing", missing)
		}
		t.Skipf("%s is missing", missing)
	}
	for _, rel := range []string{"packaging/macos/proxor-app-update.sh", "packaging/macos/tests/make-test-app.sh"} {
		if _, err := os.Stat(filepath.Join(root, rel)); err != nil {
			t.Fatalf("repository file %s: %v", rel, err)
		}
	}
	return root
}

func buildTestApp(t *testing.T, root, dir, name, version string, extra ...string) {
	t.Helper()
	args := append([]string{filepath.Join(root, "packaging/macos/tests/make-test-app.sh"), dir, name, version}, extra...)
	if out, err := exec.Command("bash", args...).CombinedOutput(); err != nil {
		t.Fatalf("make-test-app %s %s failed: %v\n%s", name, version, err, out)
	}
}

func plistVersion(t *testing.T, app string) string {
	t.Helper()
	out, err := exec.Command("/usr/libexec/PlistBuddy", "-c", "Print :CFBundleShortVersionString",
		filepath.Join(app, "Contents", "Info.plist")).Output()
	if err != nil {
		t.Fatalf("PlistBuddy on %s: %v", app, err)
	}
	return strings.TrimSpace(string(out))
}

func TestUpdateE2EMacStagesAndSwapsTheApp(t *testing.T) {
	root := macTool(t)
	suffix := "-macos-arm64.zip"
	if runtime.GOARCH == "amd64" {
		suffix = "-macos-x86_64.zip"
	}
	asset := "proxor-99.0.0" + suffix

	tmp := t.TempDir()
	apps := filepath.Join(tmp, "Apps")
	build := filepath.Join(tmp, "build")
	for _, d := range []string{apps, build} {
		if err := os.MkdirAll(d, 0o755); err != nil {
			t.Fatal(err)
		}
	}
	buildTestApp(t, root, apps, "UpdTest.app", "1.6.13")
	zipPath := filepath.Join(build, "new.zip")
	buildTestApp(t, root, build, "UpdTest.app", "99.0.0", "--zip", zipPath)
	body, err := os.ReadFile(zipPath)
	if err != nil {
		t.Fatal(err)
	}

	stageInstall(t)
	srv := newFakeReleaseServer(t, "v99.0.0", fakeAsset{name: asset, body: body})
	useFakeRelease(t, srv, "darwin", runtime.GOARCH, "1.6.13")

	stage := filepath.Join(apps, ".proxor-update")
	if err := os.Mkdir(stage, 0o755); err != nil {
		t.Fatal(err)
	}
	check := callUpdateReq(t, &gen.UpdateReq{Action: gen.UpdateAction_Check, Channel: "macos-app"})
	if check.Error != "" || check.AssetsName != asset {
		t.Fatalf("Check: asset %q error %q, want %q", check.AssetsName, check.Error, asset)
	}
	dl := callUpdateReq(t, &gen.UpdateReq{Action: gen.UpdateAction_Download, Channel: "macos-app", DownloadDir: stage})
	if dl.Error != "" {
		t.Fatalf("Download failed: %s", dl.Error)
	}
	staged := filepath.Join(stage, asset)
	got, err := os.ReadFile(staged)
	if err != nil {
		t.Fatal(err)
	}
	if sha256Hex(got) != sha256Hex(body) {
		t.Fatalf("staged zip digest %s, want %s", sha256Hex(got), sha256Hex(body))
	}

	// The fake old Proxor: a sleeping process the GUI would have quit by now.
	old := exec.Command("sleep", "30")
	if err := old.Start(); err != nil {
		t.Fatal(err)
	}
	oldDone := make(chan struct{})
	go func() { _ = old.Wait(); close(oldDone) }()
	t.Cleanup(func() { _ = old.Process.Kill(); <-oldDone })
	go func() {
		time.Sleep(time.Second)
		_ = old.Process.Kill()
	}()

	result := filepath.Join(tmp, "result.txt")
	data := filepath.Join(tmp, "data")
	target := filepath.Join(apps, "UpdTest.app")
	ctx, cancel := context.WithTimeout(context.Background(), 60*time.Second)
	defer cancel()
	cmd := exec.CommandContext(ctx, "bash", filepath.Join(root, "packaging/macos/proxor-app-update.sh"), "install",
		strconv.Itoa(old.Process.Pid), staged, target, result, "--", "-many", "-appdata", data)
	cmd.Env = append(os.Environ(), "PROXOR_APP_UPDATE_LAUNCH=exec", "PROXOR_APP_UPDATE_ALIVE_SECONDS=3")
	out, err := cmd.CombinedOutput()
	// Reap the relaunched throwaway app by the pid it logged, never by name.
	logFile := filepath.Join(apps, "UpdTest-run.log")
	t.Cleanup(func() {
		b, rerr := os.ReadFile(logFile)
		if rerr != nil {
			return
		}
		for _, line := range strings.Split(string(b), "\n") {
			f := strings.Fields(line)
			if len(f) >= 3 && f[2] == "start" && f[0] == "99.0.0" {
				if pid, perr := strconv.Atoi(f[1]); perr == nil && pid > 1 {
					_ = syscall.Kill(pid, syscall.SIGKILL)
				}
			}
		}
	})
	if err != nil {
		t.Fatalf("relauncher failed: %v\n%s", err, out)
	}

	res, err := os.ReadFile(result)
	if err != nil {
		t.Fatal(err)
	}
	if strings.TrimSpace(string(res)) != "ok 99.0.0" {
		t.Fatalf("result = %q, want \"ok 99.0.0\"\n%s", res, out)
	}
	if v := plistVersion(t, target); v != "99.0.0" {
		t.Fatalf("target version %q, want 99.0.0", v)
	}
	if b, err := exec.Command("codesign", "--verify", "--deep", "--strict", target).CombinedOutput(); err != nil {
		t.Fatalf("codesign verify failed: %v\n%s", err, b)
	}
	// Only the app (and the throwaway program's own run log, which lands beside the bundle) may remain.
	for _, n := range dirNames(t, apps) {
		if n != "UpdTest.app" && n != "UpdTest-run.log" {
			t.Fatalf("leftover in the apps folder: %q (all: %v)", n, dirNames(t, apps))
		}
	}
	logBytes, err := os.ReadFile(logFile)
	if err != nil {
		t.Fatal(err)
	}
	want := regexp.MustCompile(`(?m)^99\.0\.0 [0-9]+ start -many -appdata ` + regexp.QuoteMeta(data) + `$`)
	if !want.Match(logBytes) {
		t.Fatalf("run log lacks the relaunch line:\n%s", logBytes)
	}
}

func TestUpdateE2EMacHomebrewChannelNeverDownloads(t *testing.T) {
	suffix := "-macos-arm64.zip"
	if runtime.GOARCH == "amd64" {
		suffix = "-macos-x86_64.zip"
	}
	asset := "proxor-99.0.0" + suffix
	stageInstall(t)
	srv := newFakeReleaseServer(t, "v99.0.0", fakeAsset{name: asset, body: []byte("zip")})
	useFakeRelease(t, srv, "darwin", runtime.GOARCH, "1.6.13")
	stage := filepath.Join(t.TempDir(), ".proxor-update")
	if err := os.Mkdir(stage, 0o755); err != nil {
		t.Fatal(err)
	}
	dl := callUpdateReq(t, &gen.UpdateReq{Action: gen.UpdateAction_Download, Channel: "homebrew", DownloadDir: stage})
	if dl.Error != selfUpdateRefusal("darwin") {
		t.Fatalf("Error = %q, want the brew refusal", dl.Error)
	}
	if names := dirNames(t, stage); len(names) != 0 {
		t.Fatalf("stage folder not empty: %v", names)
	}
}

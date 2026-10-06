package grpc_server

import (
	"archive/zip"
	"bytes"
	"grpc_server/gen"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"sort"
	"strings"
	"testing"
	"time"
)

// End-to-end update path: the candidate's own Update() (check, download, SHA256SUMS verification, move into
// ../update-package.zip) runs in-process against a local fake release server, then the candidate updater applies
// the staged package into a temp install and starts the (fake) new proxor. Everything happens inside t.TempDir().

const e2eAssetName = "proxor-99.0.0-windows64.zip"

func exeName(base string) string {
	if runtime.GOOS == "windows" {
		return base + ".exe"
	}
	return base
}

// candidateUpdater returns the updater binary under test: $PROXOR_UPDATE_E2E_UPDATER or one built from
// <pkgDir>/../cmd/updater. With PROXOR_UPDATE_E2E_REQUIRE=1 the env path is mandatory (the CI gate).
func candidateUpdater(t *testing.T, pkgDir string) string {
	t.Helper()
	if p := os.Getenv("PROXOR_UPDATE_E2E_UPDATER"); p != "" {
		info, err := os.Stat(p)
		if err != nil || !info.Mode().IsRegular() {
			t.Fatalf("PROXOR_UPDATE_E2E_UPDATER=%q is not an existing file: %v", p, err)
		}
		return p
	}
	if os.Getenv("PROXOR_UPDATE_E2E_REQUIRE") == "1" {
		t.Fatal("PROXOR_UPDATE_E2E_REQUIRE=1 but PROXOR_UPDATE_E2E_UPDATER is not set: the gate must use the candidate updater")
	}
	out := filepath.Join(t.TempDir(), exeName("updater"))
	cmd := exec.Command("go", "build", "-trimpath", "-o", out, ".")
	cmd.Dir = filepath.Join(pkgDir, "..", "cmd", "updater")
	cmd.Env = append(os.Environ(), "CGO_ENABLED=0")
	if b, err := cmd.CombinedOutput(); err != nil {
		t.Fatalf("building the updater failed: %v\n%s", err, b)
	}
	return out
}

const fakeProxorSource = `package main

import (
	"os"
	"path/filepath"
)

var version = "unset"

func main() {
	exe, err := os.Executable()
	if err != nil {
		os.Exit(1)
	}
	if err := os.WriteFile(filepath.Join(filepath.Dir(exe), "launched.txt"), []byte(version), 0o644); err != nil {
		os.Exit(1)
	}
}
`

// buildFakeProxor builds a tiny program that records its version in launched.txt beside its executable.
func buildFakeProxor(t *testing.T, version string) []byte {
	t.Helper()
	dir := t.TempDir()
	if err := os.WriteFile(filepath.Join(dir, "go.mod"), []byte("module fakeproxor\n\ngo 1.21\n"), 0o644); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(dir, "main.go"), []byte(fakeProxorSource), 0o644); err != nil {
		t.Fatal(err)
	}
	out := filepath.Join(dir, exeName("fakeproxor"))
	cmd := exec.Command("go", "build", "-trimpath", "-ldflags", "-X main.version="+version, "-o", out, ".")
	cmd.Dir = dir
	cmd.Env = append(os.Environ(), "GOWORK=off", "CGO_ENABLED=0")
	if b, err := cmd.CombinedOutput(); err != nil {
		t.Fatalf("building the fake proxor failed: %v\n%s", err, b)
	}
	data, err := os.ReadFile(out)
	if err != nil {
		t.Fatal(err)
	}
	return data
}

// releaseZip returns a zip shaped like libs/package_release.sh output: everything under proxor/.
func releaseZip(t *testing.T, files map[string][]byte) []byte {
	t.Helper()
	var buf bytes.Buffer
	zw := zip.NewWriter(&buf)
	for _, d := range []string{"proxor/", "proxor/config/", "proxor/config/runtime/"} {
		hdr := &zip.FileHeader{Name: d, Method: zip.Store}
		hdr.SetMode(os.ModeDir | 0o755)
		if _, err := zw.CreateHeader(hdr); err != nil {
			t.Fatal(err)
		}
	}
	names := make([]string, 0, len(files))
	for n := range files {
		names = append(names, n)
	}
	sort.Strings(names)
	for _, n := range names {
		hdr := &zip.FileHeader{Name: "proxor/" + n, Method: zip.Deflate}
		if n == exeName("proxor") || n == exeName("updater") {
			hdr.SetMode(0o755)
		} else {
			hdr.SetMode(0o644)
		}
		w, err := zw.CreateHeader(hdr)
		if err != nil {
			t.Fatal(err)
		}
		if _, err := w.Write(files[n]); err != nil {
			t.Fatal(err)
		}
	}
	if err := zw.Close(); err != nil {
		t.Fatal(err)
	}
	return buf.Bytes()
}

type e2eInstall struct {
	pkgDir, root string
	srv          *fakeReleaseServer
	pkg          []byte
	updater      string
}

func writeInstallFile(t *testing.T, path string, data []byte) {
	t.Helper()
	if err := os.MkdirAll(filepath.Dir(path), 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(path, data, 0o755); err != nil {
		t.Fatal(err)
	}
}

// stageE2E starts the fake release server, points the update code at it and writes the old install.
// withApply also builds the candidate updater and the fake proxor binaries (download-only tests do not need them).
func stageE2E(t *testing.T, withApply bool) *e2eInstall {
	t.Helper()
	pkgDir, err := os.Getwd()
	if err != nil {
		t.Fatal(err)
	}
	in := &e2eInstall{pkgDir: pkgDir}
	oldProxor := []byte("old")
	if withApply {
		in.updater = candidateUpdater(t, pkgDir)
		updaterBytes, err := os.ReadFile(in.updater)
		if err != nil {
			t.Fatal(err)
		}
		oldProxor = buildFakeProxor(t, "old")
		in.pkg = releaseZip(t, map[string][]byte{
			exeName("proxor"):        buildFakeProxor(t, "new"),
			exeName("updater"):       updaterBytes,
			exeName("proxor_core"):   []byte("new-core"),
			"version.txt":            []byte("new"),
			"config/runtime/app.txt": []byte("runtime-new"),
		})
	} else {
		in.pkg = releaseZip(t, map[string][]byte{
			exeName("proxor"):        []byte("new"),
			"version.txt":            []byte("new"),
			"config/runtime/app.txt": []byte("runtime-new"),
		})
	}
	in.srv = newFakeReleaseServer(t, "v99.0.0", fakeAsset{name: e2eAssetName, body: in.pkg})
	useFakeRelease(t, in.srv, "windows", "amd64", "1.6.13")
	in.root = stageInstall(t)
	writeInstallFile(t, filepath.Join(in.root, exeName("proxor")), oldProxor)
	writeInstallFile(t, filepath.Join(in.root, exeName("proxor_core")), []byte("old-core"))
	writeInstallFile(t, filepath.Join(in.root, "version.txt"), []byte("old"))
	writeInstallFile(t, filepath.Join(in.root, "config", "settings.json"), []byte("user"))
	return in
}

// runUpdater starts the candidate updater the way the GUI does: copied into the install, working directory the
// install, no arguments, headless (no dialog, no browser).
func runUpdater(t *testing.T, root, updater string) {
	t.Helper()
	data, err := os.ReadFile(updater)
	if err != nil {
		t.Fatal(err)
	}
	dst := filepath.Join(root, exeName("updater"))
	if err := os.WriteFile(dst, data, 0o755); err != nil {
		t.Fatal(err)
	}
	cmd := exec.Command(dst)
	cmd.Dir = root
	var env []string
	for _, kv := range os.Environ() {
		if !strings.HasPrefix(kv, "NKR_FROM_LAUNCHER=") {
			env = append(env, kv)
		}
	}
	cmd.Env = append(env, "PROXOR_UPDATER_HEADLESS=1")
	out, err := cmd.CombinedOutput()
	if err != nil && runtime.GOOS != "windows" {
		t.Fatalf("the updater failed: %v\n%s", err, out)
	}
	t.Logf("updater output:\n%s", out)
}

func listRoot(root string) string {
	var names []string
	entries, _ := os.ReadDir(root)
	for _, e := range entries {
		names = append(names, e.Name())
	}
	return strings.Join(names, ", ")
}

func updaterOldRunning() bool {
	out, err := exec.Command("tasklist", "/FI", "IMAGENAME eq updater.old").CombinedOutput()
	return err == nil && strings.Contains(strings.ToLower(string(out)), "updater.old")
}

// waitForApplied waits until the new fake proxor recorded want in launched.txt.
func waitForApplied(t *testing.T, root, want string) {
	t.Helper()
	if runtime.GOOS == "windows" {
		t.Cleanup(func() { _ = exec.Command("taskkill", "/F", "/IM", "updater.old").Run() })
	}
	deadline := time.Now().Add(90 * time.Second)
	for {
		if b, err := os.ReadFile(filepath.Join(root, "launched.txt")); err == nil && string(b) == want {
			break
		}
		if time.Now().After(deadline) {
			errText, _ := os.ReadFile(filepath.Join(root, "update-error.txt"))
			t.Fatalf("the new proxor never started (want launched.txt %q); update-error.txt: %q; install: %s", want, errText, listRoot(root))
		}
		time.Sleep(100 * time.Millisecond)
	}
	if runtime.GOOS == "windows" {
		end := time.Now().Add(15 * time.Second)
		for updaterOldRunning() && time.Now().Before(end) {
			time.Sleep(200 * time.Millisecond)
		}
	}
}

func readText(t *testing.T, path string) string {
	t.Helper()
	b, err := os.ReadFile(path)
	if err != nil {
		t.Fatalf("reading %s: %v", path, err)
	}
	return string(b)
}

func TestUpdateE2EStagesAndAppliesAPackage(t *testing.T) {
	in := stageE2E(t, true)

	check := callUpdate(t, gen.UpdateAction_Check)
	if check.Error != "" {
		t.Fatalf("Check failed: %s", check.Error)
	}
	if check.AssetsName != e2eAssetName {
		t.Fatalf("AssetsName = %q, want %q", check.AssetsName, e2eAssetName)
	}
	if check.ReleaseUrl != in.srv.releasePage() {
		t.Fatalf("ReleaseUrl = %q, want %q", check.ReleaseUrl, in.srv.releasePage())
	}
	paths, agents := in.srv.requests()
	sawList := false
	for i, p := range paths {
		if p == "/repos/Ogstra/proxor/releases" {
			sawList = true
			if !strings.HasPrefix(agents[i], "Proxor-Updater/") {
				t.Fatalf("release list User-Agent = %q", agents[i])
			}
		}
	}
	if !sawList {
		t.Fatalf("the fake server never saw the release list request: %v", paths)
	}

	if dl := callUpdate(t, gen.UpdateAction_Download); dl.Error != "" {
		t.Fatalf("Download failed: %s", dl.Error)
	}
	staged := filepath.Join(in.root, "update-package.zip")
	if got := sha256Hex([]byte(readText(t, staged))); got != sha256Hex(in.pkg) {
		t.Fatalf("staged package digest %s, want %s", got, sha256Hex(in.pkg))
	}
	if parts := partFiles(t, in.root); len(parts) != 0 {
		t.Fatalf("leftover temp downloads: %v", parts)
	}
	if prog := callUpdate(t, gen.UpdateAction_QueryProgress); !prog.ProgressComplete || prog.Error != "" {
		t.Fatalf("progress complete=%v error=%q", prog.ProgressComplete, prog.Error)
	}

	// Like the GUI: leave the install before the updater replaces it.
	t.Chdir(t.TempDir())
	runUpdater(t, in.root, in.updater)
	waitForApplied(t, in.root, "new")

	if got := readText(t, filepath.Join(in.root, "version.txt")); got != "new" {
		t.Fatalf("version.txt = %q", got)
	}
	if got := readText(t, filepath.Join(in.root, exeName("proxor_core"))); got != "new-core" {
		t.Fatalf("%s = %q", exeName("proxor_core"), got)
	}
	if got := readText(t, filepath.Join(in.root, "config", "runtime", "app.txt")); got != "runtime-new" {
		t.Fatalf("config/runtime/app.txt = %q", got)
	}
	if got := readText(t, filepath.Join(in.root, "config", "settings.json")); got != "user" {
		t.Fatalf("config/settings.json = %q, the user config must survive", got)
	}
	for _, gone := range []string{"update-package.zip", "update-package", "update-error.txt"} {
		if _, err := os.Stat(filepath.Join(in.root, gone)); err == nil {
			t.Fatalf("%s should not exist after a successful update", gone)
		}
	}
}

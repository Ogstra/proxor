package grpc_server

import (
	"context"
	"grpc_server/gen"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

const macAppNoDirText = "Proxor could not choose a folder for the update download. Download the new Proxor.app from the release page instead."

func callUpdateReq(t *testing.T, req *gen.UpdateReq) *gen.UpdateResp {
	t.Helper()
	ret, err := (&BaseServer{}).Update(context.Background(), req)
	if err != nil {
		t.Fatalf("Update(%v) returned a Go error: %v", req.Action, err)
	}
	return ret
}

// macZipParts lists leftover proxor-*.zip.*.part names in dir (no filepath.Glob).
func macZipParts(t *testing.T, dir string) []string {
	t.Helper()
	entries, err := os.ReadDir(dir)
	if err != nil {
		t.Fatal(err)
	}
	var out []string
	for _, e := range entries {
		n := e.Name()
		if strings.HasPrefix(n, "proxor-") && strings.HasSuffix(n, ".part") {
			out = append(out, n)
		}
	}
	return out
}

func dirNames(t *testing.T, dir string) []string {
	t.Helper()
	entries, err := os.ReadDir(dir)
	if err != nil {
		t.Fatal(err)
	}
	var out []string
	for _, e := range entries {
		out = append(out, e.Name())
	}
	return out
}

func TestSelfUpdateRefusalForDarwin(t *testing.T) {
	if got := selfUpdateRefusalFor("darwin", "macos-app", "/tmp/x"); got != "" {
		t.Fatalf("macos-app with a directory = %q, want allowed", got)
	}
	got := selfUpdateRefusalFor("darwin", "macos-app", "")
	if got != macAppNoDirText {
		t.Fatalf("macos-app without a directory = %q", got)
	}
	if strings.Contains(got, "brew") {
		t.Fatalf("no-directory text must not mention brew: %q", got)
	}
	for _, c := range []string{"homebrew", "", "portable", "appimage", "unknown"} {
		got := selfUpdateRefusalFor("darwin", c, "/tmp/x")
		if got != selfUpdateRefusal("darwin") || !strings.Contains(got, "brew upgrade --cask proxor") {
			t.Fatalf("channel %q = %q, want the brew refusal", c, got)
		}
	}
}

func TestSelfUpdateRefusalForOtherSystemsIsUnchanged(t *testing.T) {
	for _, goos := range []string{"windows", "linux"} {
		for _, c := range []string{"", "portable", "macos-app", "homebrew"} {
			for _, d := range []string{"", "/tmp/x"} {
				if got := selfUpdateRefusalFor(goos, c, d); got != "" {
					t.Fatalf("%s/%q/%q = %q, want no refusal", goos, c, d, got)
				}
			}
		}
	}
}

func TestSelfUpdateRefusalDarwinTextIsPinned(t *testing.T) {
	want := "Proxor on macOS is updated by Homebrew: run brew upgrade --cask proxor (or download the macOS zip from the release page)."
	if got := selfUpdateRefusal("darwin"); got != want {
		t.Fatalf("selfUpdateRefusal(darwin) = %q", got)
	}
}

func macAppFake(t *testing.T, goarch, asset string) (*fakeReleaseServer, []byte) {
	t.Helper()
	body := []byte("fake macos zip for " + asset)
	srv := newFakeReleaseServer(t, "v99.0.0", fakeAsset{asset, body})
	srv.setSums(sha256Hex(body) + "  " + asset + "\n")
	useFakeRelease(t, srv, "darwin", goarch, "1.6.13")
	return srv, body
}

func checkMacApp(t *testing.T, asset string) {
	t.Helper()
	check := callUpdateReq(t, &gen.UpdateReq{Action: gen.UpdateAction_Check, Channel: "macos-app"})
	if check.Error != "" || check.AssetsName != asset {
		t.Fatalf("Check: asset %q error %q, want %q", check.AssetsName, check.Error, asset)
	}
}

func assertMacZipStaged(t *testing.T, dir, asset string, body []byte) {
	t.Helper()
	dl := callUpdateReq(t, &gen.UpdateReq{Action: gen.UpdateAction_Download, Channel: "macos-app", DownloadDir: dir})
	if dl.Error != "" {
		t.Fatalf("Download failed: %s", dl.Error)
	}
	got, err := os.ReadFile(filepath.Join(dir, asset))
	if err != nil {
		t.Fatal(err)
	}
	if sha256Hex(got) != sha256Hex(body) {
		t.Fatalf("staged zip digest %s, want %s", sha256Hex(got), sha256Hex(body))
	}
	if parts := macZipParts(t, dir); len(parts) != 0 {
		t.Fatalf("leftover temp downloads: %v", parts)
	}
	if prog := callUpdateReq(t, &gen.UpdateReq{Action: gen.UpdateAction_QueryProgress}); !prog.ProgressComplete || prog.Error != "" {
		t.Fatalf("progress complete=%v error=%q", prog.ProgressComplete, prog.Error)
	}
}

func TestMacAppDownloadStagesTheVerifiedZip(t *testing.T) {
	stageInstall(t)
	asset := "proxor-99.0.0-macos-arm64.zip"
	_, body := macAppFake(t, "arm64", asset)
	checkMacApp(t, asset)
	assertMacZipStaged(t, t.TempDir(), asset, body)
}

func TestMacAppIntelDownloadStagesTheX8664Zip(t *testing.T) {
	stageInstall(t)
	asset := "proxor-99.0.0-macos-x86_64.zip"
	_, body := macAppFake(t, "amd64", asset)
	checkMacApp(t, asset)
	assertMacZipStaged(t, t.TempDir(), asset, body)
}

func TestHomebrewDownloadIsStillRefused(t *testing.T) {
	stageInstall(t)
	asset := "proxor-99.0.0-macos-arm64.zip"
	macAppFake(t, "arm64", asset)
	dir := t.TempDir()
	dl := callUpdateReq(t, &gen.UpdateReq{Action: gen.UpdateAction_Download, Channel: "homebrew", DownloadDir: dir})
	if dl.Error != selfUpdateRefusal("darwin") {
		t.Fatalf("Error = %q, want the brew refusal", dl.Error)
	}
	if names := dirNames(t, dir); len(names) != 0 {
		t.Fatalf("directory not empty: %v", names)
	}
}

func TestMacAppDownloadNeedsADirectory(t *testing.T) {
	install := stageInstall(t)
	asset := "proxor-99.0.0-macos-arm64.zip"
	macAppFake(t, "arm64", asset)
	dl := callUpdateReq(t, &gen.UpdateReq{Action: gen.UpdateAction_Download, Channel: "macos-app"})
	if dl.Error != macAppNoDirText {
		t.Fatalf("Error = %q, want the no-directory text", dl.Error)
	}
	if names := dirNames(t, install); len(names) != 1 || names[0] != "config" {
		t.Fatalf("install dir changed: %v", names)
	}
	if names := dirNames(t, filepath.Join(install, "config")); len(names) != 0 {
		t.Fatalf("cwd changed: %v", names)
	}
}

func TestMacAppDownloadRejectsAMismatch(t *testing.T) {
	stageInstall(t)
	asset := "proxor-99.0.0-macos-arm64.zip"
	srv, _ := macAppFake(t, "arm64", asset)
	srv.setSums(strings.Repeat("0", 64) + "  " + asset + "\n")
	checkMacApp(t, asset)
	dir := t.TempDir()
	dl := callUpdateReq(t, &gen.UpdateReq{Action: gen.UpdateAction_Download, Channel: "macos-app", DownloadDir: dir})
	if !strings.Contains(dl.Error, "SHA-256") {
		t.Fatalf("Error = %q, want a SHA-256 mismatch", dl.Error)
	}
	if names := dirNames(t, dir); len(names) != 0 {
		t.Fatalf("directory not empty: %v", names)
	}
}

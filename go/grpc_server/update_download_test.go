package grpc_server

import (
	"errors"
	"grpc_server/gen"
	"os"
	"path/filepath"
	"regexp"
	"runtime"
	"strings"
	"testing"
	"time"
)

const fakeAssetName = "proxor-99.0.0-windows64.zip"

var fakeAssetBody = []byte("PK-fake-zip-content-for-the-update-tests")

func newStandardFake(t *testing.T) *fakeReleaseServer {
	t.Helper()
	srv := newFakeReleaseServer(t, "v99.0.0", fakeAsset{name: fakeAssetName, body: fakeAssetBody})
	useFakeRelease(t, srv, "windows", "amd64", "1.6.13")
	return srv
}

func failRename(t *testing.T) {
	t.Helper()
	old := renameFile
	renameFile = func(from, to string) error {
		return errors.New("The process cannot access the file because it is being used by another process.")
	}
	t.Cleanup(func() { renameFile = old })
}

func TestRenameRetryScheduleIsPinned(t *testing.T) {
	if renameAttempts != 12 {
		t.Fatalf("renameAttempts = %d, want 12", renameAttempts)
	}
	want := []time.Duration{100, 200, 400, 800, 1600, 2000, 2000, 2000, 2000, 2000, 2000}
	var total time.Duration
	for n, ms := range want {
		got := renameRetryDelay(n)
		if got != ms*time.Millisecond {
			t.Fatalf("renameRetryDelay(%d) = %v, want %v", n, got, ms*time.Millisecond)
		}
		total += got
	}
	if total < 10*time.Second || total > 20*time.Second {
		t.Fatalf("total wait %v outside 10s..20s", total)
	}
}

func TestRenameWithRetryWaitsOnlyBetweenAttempts(t *testing.T) {
	want := errors.New("locked")
	calls := 0
	old := renameFile
	renameFile = func(from, to string) error { calls++; return want }
	defer func() { renameFile = old }()
	waits := stubRetrySleep(t)

	if err := renameWithRetry("a.part", "a.zip"); err != want {
		t.Fatalf("expected the last error unchanged, got %v", err)
	}
	if calls != renameAttempts {
		t.Fatalf("calls = %d, want %d", calls, renameAttempts)
	}
	if len(*waits) != renameAttempts-1 {
		t.Fatalf("waits = %d, want %d", len(*waits), renameAttempts-1)
	}
	for n, d := range *waits {
		if d != renameRetryDelay(n) {
			t.Fatalf("wait %d = %v, want %v", n, d, renameRetryDelay(n))
		}
	}
}

func TestRemoveStaleDownloadsRemovesLegacyAndUniqueParts(t *testing.T) {
	dir := t.TempDir()
	for _, n := range []string{"update-package.zip.part", "update-package.zip.123.part", "update-package.zip", "other.part"} {
		if err := os.WriteFile(filepath.Join(dir, n), []byte("x"), 0o644); err != nil {
			t.Fatal(err)
		}
	}
	removeStaleDownloads(filepath.Join(dir, "update-package.zip"))
	for n, wantGone := range map[string]bool{
		"update-package.zip.part": true, "update-package.zip.123.part": true,
		"update-package.zip": false, "other.part": false,
	} {
		_, err := os.Stat(filepath.Join(dir, n))
		if gone := os.IsNotExist(err); gone != wantGone {
			t.Fatalf("%s: removed=%v, want %v", n, gone, wantGone)
		}
	}
}

func TestRemoveStaleDownloadsHandlesGlobMetaCharacters(t *testing.T) {
	dir := filepath.Join(t.TempDir(), "a[1]*b")
	if err := os.MkdirAll(dir, 0o755); err != nil {
		t.Fatal(err)
	}
	for _, n := range []string{"update-package.zip.part", "update-package.zip.77.part", "keep.txt"} {
		if err := os.WriteFile(filepath.Join(dir, n), []byte("x"), 0o644); err != nil {
			t.Fatal(err)
		}
	}
	removeStaleDownloads(filepath.Join(dir, "update-package.zip"))
	if got := partFiles(t, dir); len(got) != 0 {
		t.Fatalf("stale parts left: %v", got)
	}
	if _, err := os.Stat(filepath.Join(dir, "keep.txt")); err != nil {
		t.Fatalf("unrelated file removed: %v", err)
	}
}

func writeTemp(t *testing.T, body []byte) (temp, dest string) {
	t.Helper()
	dir := t.TempDir()
	temp = filepath.Join(dir, "update-package.zip.1.part")
	dest = filepath.Join(dir, "update-package.zip")
	if err := os.WriteFile(temp, body, 0o644); err != nil {
		t.Fatal(err)
	}
	return temp, dest
}

func TestMoveVerifiedDownloadRenamesWhenPossible(t *testing.T) {
	temp, dest := writeTemp(t, fakeAssetBody)
	if err := moveVerifiedDownload(temp, dest, sha256Hex(fakeAssetBody)); err != nil {
		t.Fatal(err)
	}
	got, err := os.ReadFile(dest)
	if err != nil || string(got) != string(fakeAssetBody) {
		t.Fatalf("destination content wrong: %v %q", err, got)
	}
	if _, err := os.Stat(temp); !os.IsNotExist(err) {
		t.Fatalf("temp file still present: %v", err)
	}
}

func TestMoveVerifiedDownloadFallsBackToCopy(t *testing.T) {
	temp, dest := writeTemp(t, fakeAssetBody)
	failRename(t)
	stubRetrySleep(t)
	if err := moveVerifiedDownload(temp, dest, sha256Hex(fakeAssetBody)); err != nil {
		t.Fatal(err)
	}
	got, err := os.ReadFile(dest)
	if err != nil || string(got) != string(fakeAssetBody) {
		t.Fatalf("destination content wrong: %v %q", err, got)
	}
	if _, err := os.Stat(temp); !os.IsNotExist(err) {
		t.Fatalf("temp file still present: %v", err)
	}
}

func TestMoveVerifiedDownloadCopyMustMatchTheDigest(t *testing.T) {
	temp, dest := writeTemp(t, fakeAssetBody)
	failRename(t)
	stubRetrySleep(t)
	err := moveVerifiedDownload(temp, dest, sha256Hex([]byte("something else")))
	if err == nil || !strings.Contains(err.Error(), "SHA-256") {
		t.Fatalf("expected a SHA-256 error, got %v", err)
	}
	if _, statErr := os.Stat(dest); !os.IsNotExist(statErr) {
		t.Fatalf("destination must not exist: %v", statErr)
	}
}

func TestMoveVerifiedDownloadErrorNamesPathsAndCause(t *testing.T) {
	dir := t.TempDir()
	temp := filepath.Join(dir, "update-package.zip.1.part")
	dest := filepath.Join(dir, "update-package.zip")
	if err := os.WriteFile(temp, fakeAssetBody, 0o644); err != nil {
		t.Fatal(err)
	}
	if err := os.MkdirAll(dest, 0o755); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(dest, "inside"), []byte("x"), 0o644); err != nil {
		t.Fatal(err)
	}
	stubRetrySleep(t)

	err := moveVerifiedDownload(temp, dest, sha256Hex(fakeAssetBody))
	if err == nil {
		t.Fatal("expected an error")
	}
	absTemp, _ := filepath.Abs(temp)
	absDest, _ := filepath.Abs(dest)
	for _, want := range []string{absTemp, absDest, "after 12 attempts", "copying it there instead also failed", "antivirus"} {
		if !strings.Contains(err.Error(), want) {
			t.Fatalf("error %q does not contain %q", err.Error(), want)
		}
	}
	if _, statErr := os.Stat(temp); statErr != nil {
		t.Fatalf("temp must survive a failed move: %v", statErr)
	}
}

func TestCheckUsesTheAPIBaseURLSeam(t *testing.T) {
	srv := newStandardFake(t)
	ret := callUpdate(t, gen.UpdateAction_Check)
	if ret.Error != "" {
		t.Fatalf("check error: %s", ret.Error)
	}
	if ret.AssetsName != fakeAssetName || ret.ReleaseUrl != srv.releasePage() {
		t.Fatalf("unexpected check result: %q %q", ret.AssetsName, ret.ReleaseUrl)
	}
	paths, agents := srv.requests()
	if len(paths) == 0 || paths[0] != "/repos/Ogstra/proxor/releases" {
		t.Fatalf("paths = %v", paths)
	}
	if !strings.HasPrefix(agents[0], "Proxor-Updater/") {
		t.Fatalf("user agent = %q", agents[0])
	}
}

func TestDownloadUsesAUniqueTempNameAndCleansStaleParts(t *testing.T) {
	srv := newStandardFake(t)
	install := stageInstall(t)
	for _, n := range []string{"update-package.zip.part", "update-package.zip.999.part"} {
		if err := os.WriteFile(filepath.Join(install, n), []byte("stale"), 0o644); err != nil {
			t.Fatal(err)
		}
	}
	var froms []string
	old := renameFile
	renameFile = func(from, to string) error { froms = append(froms, from); return old(from, to) }
	defer func() { renameFile = old }()

	if ret := callUpdate(t, gen.UpdateAction_Check); ret.Error != "" {
		t.Fatalf("check: %s", ret.Error)
	}
	if ret := callUpdate(t, gen.UpdateAction_Download); ret.Error != "" {
		t.Fatalf("download: %s", ret.Error)
	}
	_ = srv
	got, err := os.ReadFile(filepath.Join(install, "update-package.zip"))
	if err != nil || string(got) != string(fakeAssetBody) {
		t.Fatalf("package wrong: %v", err)
	}
	if len(froms) == 0 || !regexp.MustCompile(`update-package\.zip\.[^.]+\.part$`).MatchString(froms[0]) ||
		strings.HasSuffix(froms[0], "update-package.zip.part") {
		t.Fatalf("rename source = %v", froms)
	}
	if left := partFiles(t, install); len(left) != 0 {
		t.Fatalf("parts left: %v", left)
	}
	p := callUpdate(t, gen.UpdateAction_QueryProgress)
	if !p.ProgressComplete || p.ProgressReceived != int64(len(fakeAssetBody)) {
		t.Fatalf("progress = %+v", p)
	}
}

func successfulDownload(t *testing.T) {
	t.Helper()
	if ret := callUpdate(t, gen.UpdateAction_Check); ret.Error != "" {
		t.Fatalf("check: %s", ret.Error)
	}
	if ret := callUpdate(t, gen.UpdateAction_Download); ret.Error != "" {
		t.Fatalf("download: %s", ret.Error)
	}
	if p := callUpdate(t, gen.UpdateAction_QueryProgress); !p.ProgressComplete {
		t.Fatalf("first download not complete: %+v", p)
	}
}

func TestDownloadFailureIsReportedToTheProgressPoll(t *testing.T) {
	srv := newStandardFake(t)
	stageInstall(t)
	successfulDownload(t)

	srv.setAssetStatus(500)
	callUpdate(t, gen.UpdateAction_Check)
	ret := callUpdate(t, gen.UpdateAction_Download)
	if !strings.Contains(ret.Error, "status 500") {
		t.Fatalf("download error = %q", ret.Error)
	}
	p := callUpdate(t, gen.UpdateAction_QueryProgress)
	if p.ProgressComplete || !strings.Contains(p.Error, "status 500") {
		t.Fatalf("progress = %+v", p)
	}
}

func TestDownloadNotQueuedIsReportedToTheProgressPoll(t *testing.T) {
	newStandardFake(t)
	stageInstall(t)
	successfulDownload(t)

	updateDownloadURL, updateAssetName = "", ""
	ret := callUpdate(t, gen.UpdateAction_Download)
	const want = "No update package is queued for download."
	if ret.Error != want {
		t.Fatalf("download error = %q", ret.Error)
	}
	p := callUpdate(t, gen.UpdateAction_QueryProgress)
	if p.ProgressComplete || p.Error != want {
		t.Fatalf("progress = %+v", p)
	}
}

func TestDownloadChecksumMismatchKeepsNothing(t *testing.T) {
	srv := newStandardFake(t)
	install := stageInstall(t)
	srv.setSums(sha256Hex([]byte("other")) + "  ./" + fakeAssetName + "\n")

	callUpdate(t, gen.UpdateAction_Check)
	ret := callUpdate(t, gen.UpdateAction_Download)
	if !strings.Contains(ret.Error, "does not match") {
		t.Fatalf("download error = %q", ret.Error)
	}
	if _, err := os.Stat(filepath.Join(install, "update-package.zip")); !os.IsNotExist(err) {
		t.Fatalf("package must not exist: %v", err)
	}
	if left := partFiles(t, install); len(left) != 0 {
		t.Fatalf("parts left: %v", left)
	}
}

func TestCheckClearsAStaleDownloadError(t *testing.T) {
	srv := newStandardFake(t)
	stageInstall(t)
	srv.setAssetStatus(500)
	callUpdate(t, gen.UpdateAction_Check)
	callUpdate(t, gen.UpdateAction_Download)
	if p := callUpdate(t, gen.UpdateAction_QueryProgress); p.Error == "" {
		t.Fatal("expected the failed download to be reported")
	}
	if ret := callUpdate(t, gen.UpdateAction_Check); ret.Error != "" {
		t.Fatalf("check: %s", ret.Error)
	}
	if p := callUpdate(t, gen.UpdateAction_QueryProgress); p.Error != "" || p.ProgressComplete {
		t.Fatalf("progress not reset: %+v", p)
	}
}

func TestUpdateSeamsAreOnlyAssignedInTests(t *testing.T) {
	if updateAPIBaseURL != "https://api.github.com" || updateGOOS != runtime.GOOS || updateGOARCH != runtime.GOARCH {
		t.Fatalf("production seam values changed: %q %q %q", updateAPIBaseURL, updateGOOS, updateGOARCH)
	}
	assign := regexp.MustCompile(`(?m)^\s*(updateAPIBaseURL|updateGOOS|updateGOARCH|retrySleep)\s*=`)
	decl := regexp.MustCompile(`(?m)^var (updateAPIBaseURL|updateGOOS|updateGOARCH|retrySleep) = `)
	files, err := os.ReadDir(".")
	if err != nil {
		t.Fatal(err)
	}
	declared := 0
	for _, f := range files {
		n := f.Name()
		if !strings.HasSuffix(n, ".go") || strings.HasSuffix(n, "_test.go") {
			continue
		}
		b, err := os.ReadFile(n)
		if err != nil {
			t.Fatal(err)
		}
		if assign.Match(b) {
			t.Fatalf("%s assigns a test seam", n)
		}
		declared += len(decl.FindAll(b, -1))
	}
	if declared != 4 {
		t.Fatalf("seam declarations = %d, want 4", declared)
	}
}

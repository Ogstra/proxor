package grpc_server

import (
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func TestParseReleaseVersion(t *testing.T) {
	version, ok := parseReleaseVersion("proxor-1.0.3-windows64.zip")
	if !ok {
		t.Fatal("expected version to parse")
	}
	if version.major != 1 || version.minor != 0 || version.patch != 3 {
		t.Fatalf("unexpected version components: %+v", version)
	}
}

func TestUpdateArchiveSuffixes(t *testing.T) {
	tests := []struct {
		goos     string
		goarch   string
		expected []string
		wantErr  bool
	}{
		{goos: "windows", goarch: "amd64", expected: []string{"windows64.zip"}},
		{goos: "windows", goarch: "arm64", expected: []string{"windows-arm64.zip"}},
		{goos: "linux", goarch: "amd64", expected: []string{"linux64.AppImage"}},
		{goos: "linux", goarch: "arm64", wantErr: true},
		{goos: "darwin", goarch: "amd64", wantErr: true},
	}

	for _, tt := range tests {
		got, err := updateArchiveSuffixes(tt.goos, tt.goarch)
		if tt.wantErr {
			if err == nil {
				t.Fatalf("%s/%s: expected error", tt.goos, tt.goarch)
			}
			continue
		}
		if err != nil {
			t.Fatalf("%s/%s: unexpected error: %v", tt.goos, tt.goarch, err)
		}
		if len(got) != len(tt.expected) {
			t.Fatalf("%s/%s: expected %v, got %v", tt.goos, tt.goarch, tt.expected, got)
		}
		for i := range got {
			if got[i] != tt.expected[i] {
				t.Fatalf("%s/%s: expected %v, got %v", tt.goos, tt.goarch, tt.expected, got)
			}
		}
	}
}

// TestLinuxUpdateCheckResolvesAppImageAsset replaces the old guidance-text contract: the
// stopgap that refused self-update for all of Linux is gone, and this package no longer
// produces any channel's guidance wording -- that now lives in src/main/PackagePolicy.cpp.
func TestLinuxUpdateCheckResolvesAppImageAsset(t *testing.T) {
	got, err := updateArchiveSuffixes("linux", "amd64")
	if err != nil {
		t.Fatalf("linux/amd64: expected no error, got %v", err)
	}
	if len(got) != 1 || got[0] != "linux64.AppImage" {
		t.Fatalf("linux/amd64: expected [linux64.AppImage], got %v", got)
	}
}

func TestSuffixesForChannel(t *testing.T) {
	tests := []struct {
		channel  string
		goos     string
		goarch   string
		expected []string
		wantErr  bool
	}{
		{channel: "deb", goos: "linux", goarch: "amd64", expected: []string{"_amd64.deb"}},
		{channel: "rpm", goos: "linux", goarch: "amd64", expected: []string{".x86_64.rpm"}},
		{channel: "flatpak", goos: "linux", goarch: "amd64", expected: []string{".flatpak"}},
		{channel: "winget", goos: "linux", goarch: "amd64", expected: []string{"winget-x64.zip"}},
		{channel: "arch", goos: "linux", goarch: "amd64", expected: []string{".tar.gz"}},
		{channel: "appimage", goos: "linux", goarch: "amd64", expected: []string{"linux64.AppImage"}},
		{channel: "portable", goos: "linux", goarch: "amd64", expected: []string{"linux64.AppImage"}},
		// Empty/unrecognised channel falls back to updateArchiveSuffixes, so an older
		// GUI against a newer core still works.
		{channel: "", goos: "linux", goarch: "amd64", expected: []string{"linux64.AppImage"}},
		{channel: "unknown-package-manager", goos: "linux", goarch: "amd64", expected: []string{"linux64.AppImage"}},
		{channel: "", goos: "linux", goarch: "arm64", wantErr: true},
		{channel: "deb", goos: "windows", goarch: "amd64", expected: []string{"windows64.zip"}},
	}

	for _, tt := range tests {
		got, err := suffixesForChannel(tt.channel, tt.goos, tt.goarch)
		if tt.wantErr {
			if err == nil {
				t.Fatalf("channel=%q %s/%s: expected error", tt.channel, tt.goos, tt.goarch)
			}
			continue
		}
		if err != nil {
			t.Fatalf("channel=%q %s/%s: unexpected error: %v", tt.channel, tt.goos, tt.goarch, err)
		}
		if len(got) != len(tt.expected) {
			t.Fatalf("channel=%q %s/%s: expected %v, got %v", tt.channel, tt.goos, tt.goarch, tt.expected, got)
		}
		for i := range got {
			if got[i] != tt.expected[i] {
				t.Fatalf("channel=%q %s/%s: expected %v, got %v", tt.channel, tt.goos, tt.goarch, tt.expected, got)
			}
		}
	}
}

// Every suffix above matches an asset name actually published in v1.6.7, and
// parseReleaseVersion extracts 1.6.7 from each.
func TestSuffixesForChannelMatchRealV167AssetNames(t *testing.T) {
	realAssetNames := map[string]string{
		"linux64.AppImage": "proxor-1.6.7-linux64.AppImage",
		"_amd64.deb":       "proxor_1.6.7-1_amd64.deb",
		".x86_64.rpm":      "proxor-1.6.7-1.fc44.x86_64.rpm",
		".flatpak":         "proxor-1.6.7.flatpak",
		"winget-x64.zip":   "proxor-1.6.7-winget-x64.zip",
		".tar.gz":          "proxor-1.6.7.tar.gz",
	}
	for suffix, assetName := range realAssetNames {
		if !strings.HasSuffix(assetName, suffix) {
			t.Fatalf("asset %q does not carry suffix %q", assetName, suffix)
		}
		version, ok := parseReleaseVersion(assetName)
		if !ok {
			t.Fatalf("parseReleaseVersion could not parse %q", assetName)
		}
		if version.major != 1 || version.minor != 6 || version.patch != 7 {
			t.Fatalf("asset %q parsed as %+v, want 1.6.7", assetName, version)
		}
	}
}

func TestMatchingReleaseAssetNoUpdateWhenCurrentVersionMatches(t *testing.T) {
	releases := []githubRelease{
		{
			TagName: "proxor-1.0.1",
			Assets: []githubReleaseAsset{
				{Name: "proxor-1.0.1-windows64.zip"},
			},
		},
	}

	release, asset, selection := matchingReleaseAsset(releases, "proxor-1.0.1", []string{"windows64.zip"}, false)
	if release != nil || asset != nil || selection != updateSelectionCurrent {
		t.Fatalf("expected current-version match, got release=%v asset=%v selection=%v", release, asset, selection)
	}
}

func TestMatchingReleaseAssetSkipsPrereleaseByDefault(t *testing.T) {
	releases := []githubRelease{
		{
			TagName:    "proxor-1.0.2",
			Prerelease: true,
			Assets: []githubReleaseAsset{
				{Name: "proxor-1.0.2-windows64.zip", BrowserDownloadURL: "https://example.com/pre.zip"},
			},
		},
		{
			TagName: "proxor-1.0.1",
			Assets: []githubReleaseAsset{
				{Name: "proxor-1.0.1-windows64.zip", BrowserDownloadURL: "https://example.com/current.zip"},
			},
		},
	}

	release, asset, selection := matchingReleaseAsset(releases, "proxor-1.0.1", []string{"windows64.zip"}, false)
	if release != nil || asset != nil || selection != updateSelectionCurrent {
		t.Fatalf("expected current-version match when prereleases are skipped, got release=%v asset=%v selection=%v", release, asset, selection)
	}
}

func TestMatchingReleaseAssetFindsNewStableRelease(t *testing.T) {
	releases := []githubRelease{
		{
			TagName: "proxor-1.0.2",
			HTMLURL: "https://example.com/release",
			Assets: []githubReleaseAsset{
				{Name: "proxor-1.0.2-windows64.zip", BrowserDownloadURL: "https://example.com/update.zip"},
			},
		},
	}

	release, asset, selection := matchingReleaseAsset(releases, "proxor-1.0.1", []string{"windows64.zip"}, false)
	if release == nil || asset == nil || selection != updateSelectionAvailable {
		t.Fatalf("expected an update candidate")
	}
	if asset.Name != "proxor-1.0.2-windows64.zip" {
		t.Fatalf("unexpected asset: %s", asset.Name)
	}
}

func TestMatchingReleaseAssetNeverDowngradesWhenOlderReleaseAppearsFirst(t *testing.T) {
	releases := []githubRelease{
		{
			TagName: "proxor-1.0.2",
			Assets: []githubReleaseAsset{
				{Name: "proxor-1.0.2-windows64.zip", BrowserDownloadURL: "https://example.com/102.zip"},
			},
		},
		{
			TagName: "proxor-1.0.3",
			Assets: []githubReleaseAsset{
				{Name: "proxor-1.0.3-windows64.zip", BrowserDownloadURL: "https://example.com/103.zip"},
			},
		},
	}

	release, asset, selection := matchingReleaseAsset(releases, "proxor-1.0.3", []string{"windows64.zip"}, false)
	if release != nil || asset != nil || selection != updateSelectionCurrent {
		t.Fatalf("expected current-version match instead of downgrade, got release=%v asset=%v selection=%v", release, asset, selection)
	}
}

func TestMatchingReleaseAssetChoosesHighestCompatibleVersion(t *testing.T) {
	releases := []githubRelease{
		{
			TagName: "proxor-1.0.2",
			Assets: []githubReleaseAsset{
				{Name: "proxor-1.0.2-windows64.zip", BrowserDownloadURL: "https://example.com/102.zip"},
			},
		},
		{
			TagName: "proxor-1.0.4",
			Assets: []githubReleaseAsset{
				{Name: "proxor-1.0.4-windows64.zip", BrowserDownloadURL: "https://example.com/104.zip"},
			},
		},
		{
			TagName: "proxor-1.0.3",
			Assets: []githubReleaseAsset{
				{Name: "proxor-1.0.3-windows64.zip", BrowserDownloadURL: "https://example.com/103.zip"},
			},
		},
	}

	release, asset, selection := matchingReleaseAsset(releases, "proxor-1.0.1", []string{"windows64.zip"}, false)
	if release == nil || asset == nil || selection != updateSelectionAvailable {
		t.Fatalf("expected an update candidate")
	}
	if release.TagName != "proxor-1.0.4" || asset.Name != "proxor-1.0.4-windows64.zip" {
		t.Fatalf("expected highest compatible release, got release=%v asset=%v", release.TagName, asset.Name)
	}
}

func TestMatchingReleaseAssetReportsNoCompatiblePackage(t *testing.T) {
	releases := []githubRelease{
		{
			TagName: "proxor-1.0.2",
			Assets: []githubReleaseAsset{
				{Name: "proxor-1.0.2-linux64.zip"},
			},
		},
	}

	release, asset, selection := matchingReleaseAsset(releases, "proxor-1.0.1", []string{"windows64.zip"}, false)
	if release != nil || asset != nil || selection != updateSelectionNoCompatible {
		t.Fatalf("expected no compatible package, got release=%v asset=%v selection=%v", release, asset, selection)
	}
}

func TestMatchingReleaseAssetDoesNotDowngradeFromPrereleaseCurrent(t *testing.T) {
	releases := []githubRelease{
		{
			TagName: "proxor-1.6.0",
			Assets: []githubReleaseAsset{
				{Name: "proxor-1.6.0-windows64.zip", BrowserDownloadURL: "https://example.com/154.zip"},
			},
		},
	}

	release, asset, selection := matchingReleaseAsset(releases, "v1.6-beta-1", []string{"windows64.zip"}, false)
	if release != nil || asset != nil || selection != updateSelectionCurrent {
		t.Fatalf("expected current-version match, got release=%v asset=%v selection=%v", release, asset, selection)
	}
}

func TestMatchingReleaseAssetAllowsNewPrereleaseWhenEnabled(t *testing.T) {
	releases := []githubRelease{
		{
			TagName:    "proxor-1.6-beta-2",
			Prerelease: true,
			Assets: []githubReleaseAsset{
				{Name: "proxor-1.6-beta-2-windows64.zip", BrowserDownloadURL: "https://example.com/beta2.zip"},
			},
		},
	}

	release, asset, selection := matchingReleaseAsset(releases, "v1.6-beta-1", []string{"windows64.zip"}, true)
	if release == nil || asset == nil || selection != updateSelectionAvailable {
		t.Fatalf("expected prerelease update, got release=%v asset=%v selection=%v", release, asset, selection)
	}
	if release.TagName != "proxor-1.6-beta-2" || asset.Name != "proxor-1.6-beta-2-windows64.zip" {
		t.Fatalf("unexpected prerelease candidate: release=%v asset=%v", release.TagName, asset.Name)
	}
}

func TestDownloadDestinationEmptyDirKeepsExistingBehaviour(t *testing.T) {
	got, err := downloadDestination("", "proxor-1.6.7-linux64.AppImage")
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	want := downloadedArchivePath("proxor-1.6.7-linux64.AppImage")
	if got != want {
		t.Fatalf("got %q, want %q", got, want)
	}
}

func TestDownloadDestinationUsesChosenDirectory(t *testing.T) {
	dir := t.TempDir()
	got, err := downloadDestination(dir, "proxor-1.6.7-linux64.AppImage")
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	want := filepath.Join(dir, "proxor-1.6.7-linux64.AppImage")
	if got != want {
		t.Fatalf("got %q, want %q", got, want)
	}
}

func TestDownloadDestinationRejectsMissingDirectory(t *testing.T) {
	missing := filepath.Join(t.TempDir(), "does-not-exist")
	_, err := downloadDestination(missing, "proxor-1.6.7-linux64.AppImage")
	if err == nil {
		t.Fatal("expected an error naming the missing directory")
	}
	if !strings.Contains(err.Error(), missing) {
		t.Fatalf("error %q does not name the directory %q", err.Error(), missing)
	}
}

func TestDownloadDestinationRejectsAFile(t *testing.T) {
	dir := t.TempDir()
	notADir := filepath.Join(dir, "not-a-dir")
	if err := os.WriteFile(notADir, []byte("x"), 0644); err != nil {
		t.Fatalf("failed to create fixture file: %v", err)
	}
	_, err := downloadDestination(notADir, "proxor-1.6.7-linux64.AppImage")
	if err == nil {
		t.Fatal("expected an error naming the non-directory path")
	}
	if !strings.Contains(err.Error(), notADir) {
		t.Fatalf("error %q does not name the path %q", err.Error(), notADir)
	}
}

// Inline fixture copied from the real v1.6.7 SHA256SUMS shape: GNU coreutils style,
// hash, two spaces, then the name with a leading "./", one asset per line.
const v167Sums = "c4de65072e34783fa1244c85a6e2e677c65110b7e51fe403483392111327e51b  ./proxor-1.6.7-linux64.AppImage\n" +
	"293d7ab1512b8cf1fe1781ecb0530c4db71059e07640421b490546bc083d6b9f  ./proxor_1.6.7-1_amd64.deb\n"

func TestChecksumForAssetFindsHashWithLeadingDotSlash(t *testing.T) {
	got, err := checksumForAsset(v167Sums, "proxor-1.6.7-linux64.AppImage")
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	want := "c4de65072e34783fa1244c85a6e2e677c65110b7e51fe403483392111327e51b"
	if got != want {
		t.Fatalf("got %q, want %q", got, want)
	}
}

func TestChecksumForAssetMatchesOnBaseNameOnly(t *testing.T) {
	// A browser_download_url-shaped wanted name must never leak into the lookup key.
	got, err := checksumForAsset(v167Sums, "https://github.com/Ogstra/proxor/releases/download/v1.6.7/proxor-1.6.7-linux64.AppImage")
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	want := "c4de65072e34783fa1244c85a6e2e677c65110b7e51fe403483392111327e51b"
	if got != want {
		t.Fatalf("got %q, want %q", got, want)
	}
}

func TestChecksumForAssetToleratesBareNameBinaryMarkerBlankLinesAndCR(t *testing.T) {
	sums := "\r\n" +
		"deadbeef00000000000000000000000000000000000000000000000000beef proxor-1.6.7-linux64.AppImage\r\n" +
		"\n" +
		"cafebabe00000000000000000000000000000000000000000000000000babe *proxor_1.6.7-1_amd64.deb\r\n"
	got, err := checksumForAsset(sums, "proxor-1.6.7-linux64.AppImage")
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if got != "deadbeef00000000000000000000000000000000000000000000000000beef" {
		t.Fatalf("unexpected digest: %q", got)
	}
	got, err = checksumForAsset(sums, "proxor_1.6.7-1_amd64.deb")
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if got != "cafebabe00000000000000000000000000000000000000000000000000babe" {
		t.Fatalf("unexpected digest: %q", got)
	}
}

func TestChecksumForAssetFailsClosedWhenAssetIsAbsent(t *testing.T) {
	got, err := checksumForAsset(v167Sums, "proxor-1.6.7.flatpak")
	if err == nil {
		t.Fatal("expected an error, got none")
	}
	if got != "" {
		t.Fatalf("expected an empty digest on error, got %q", got)
	}
}

func TestChecksumForAssetFailsClosedOnAmbiguousDuplicate(t *testing.T) {
	sums := "aaaa  ./proxor-1.6.7-linux64.AppImage\n" +
		"bbbb  ./proxor-1.6.7-linux64.AppImage\n"
	_, err := checksumForAsset(sums, "proxor-1.6.7-linux64.AppImage")
	if err == nil {
		t.Fatal("expected an error for two different hashes on the same asset")
	}
}

func TestChecksumForAssetAllowsDuplicateIdenticalLines(t *testing.T) {
	sums := "aaaa  ./proxor-1.6.7-linux64.AppImage\n" +
		"aaaa  ./proxor-1.6.7-linux64.AppImage\n"
	got, err := checksumForAsset(sums, "proxor-1.6.7-linux64.AppImage")
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if got != "aaaa" {
		t.Fatalf("got %q, want %q", got, "aaaa")
	}
}

func TestVerifyAssetChecksumPassesOnMatch(t *testing.T) {
	err := verifyAssetChecksum(v167Sums, "proxor-1.6.7-linux64.AppImage",
		"C4DE65072E34783FA1244C85A6E2E677C65110B7E51FE403483392111327E51B")
	if err != nil {
		t.Fatalf("expected a case-insensitive match, got %v", err)
	}
}

// A download whose computed digest differs from the published one must fail -- this is
// the mismatch path: a deliberately corrupted asset cannot be produced against a
// published release, so this comparison is what proves it, not a live download.
func TestVerifyAssetChecksumFailsOnMismatch(t *testing.T) {
	err := verifyAssetChecksum(v167Sums, "proxor-1.6.7-linux64.AppImage",
		"00000000000000000000000000000000000000000000000000000000000000")
	if err == nil {
		t.Fatal("expected an error for a digest mismatch")
	}
}

func TestVerifyAssetChecksumFailsWhenAssetHasNoEntry(t *testing.T) {
	err := verifyAssetChecksum(v167Sums, "proxor-1.6.7.flatpak", "anything")
	if err == nil {
		t.Fatal("expected an error when the wanted asset has no SHA256SUMS line")
	}
}

// A release that publishes no SHA256SUMS asset at all must fail the download rather
// than apply it unverified -- verifyAssetChecksum is never even reached in that case
// (see the Download case's own updateChecksumsURL == "" check), but an empty sums body
// is the degenerate form of "nothing to verify against" and must still fail closed.
func TestVerifyAssetChecksumFailsOnEmptySums(t *testing.T) {
	err := verifyAssetChecksum("", "proxor-1.6.7-linux64.AppImage", "anything")
	if err == nil {
		t.Fatal("expected an error for an empty SHA256SUMS body")
	}
}

func TestUpdateRepoConstants(t *testing.T) {
	if updateRepoName != "proxor" {
		t.Fatalf("updateRepoName = %q, want %q", updateRepoName, "proxor")
	}
	if updateRepoOwner != "Ogstra" {
		t.Fatalf("updateRepoOwner = %q, want %q", updateRepoOwner, "Ogstra")
	}
}

func TestUpdateUserAgent(t *testing.T) {
	const want = "Proxor-Updater/"
	if updateUserAgentPrefix != want {
		t.Fatalf("updateUserAgentPrefix = %q, want %q", updateUserAgentPrefix, want)
	}
}

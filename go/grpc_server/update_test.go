package grpc_server

import "testing"

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
		{goos: "linux", goarch: "amd64", expected: []string{"linux64.zip", "linux64.tar.gz"}},
		{goos: "linux", goarch: "arm64", expected: []string{"linux-arm64.zip", "linux-arm64.tar.gz"}},
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

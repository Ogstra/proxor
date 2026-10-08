package grpc_server

import "testing"

func TestArchIgnoresWindowsOnlyRelease(t *testing.T) {
	mk := func(tag string, pre bool, names ...string) githubRelease {
		r := githubRelease{TagName: tag, Prerelease: pre}
		for _, n := range names {
			r.Assets = append(r.Assets, githubReleaseAsset{Name: n})
		}
		return r
	}
	rels := []githubRelease{
		mk("v1.6.13", false, "proxor-1.6.13-windows64.zip", "proxor-1.6.13-winget-x64.zip", "proxor-1.6.13.tar.gz", "SHA256SUMS"),
		mk("v1.6.12", true, "proxor-1.6.12-linux64.AppImage", "proxor-1.6.12-windows64.zip", "proxor-1.6.12.tar.gz", "SHA256SUMS"),
	}
	suffixes, err := suffixesForChannel("arch", "linux", "amd64")
	if err != nil {
		t.Fatal(err)
	}
	// Without the filter the Windows-only tarball would be offered to an Arch user on 1.6.12.
	if _, _, sel := matchingReleaseAsset(rels, "1.6.12", suffixes, true); sel != updateSelectionAvailable {
		t.Fatalf("precondition: the unfiltered list offers 1.6.13, got selection %v", sel)
	}
	filtered := releasesWithAsset(rels, "linux64.AppImage")
	if len(filtered) != 1 || filtered[0].TagName != "v1.6.12" {
		t.Fatalf("only the release with the Linux build may remain, got %+v", filtered)
	}
	if _, _, sel := matchingReleaseAsset(filtered, "1.6.12", suffixes, true); sel == updateSelectionAvailable {
		t.Fatalf("an Arch user on 1.6.12 must not be offered the Windows-only release, got selection %v", sel)
	}
	if rel, _, sel := matchingReleaseAsset(filtered, "1.6.11", suffixes, true); sel != updateSelectionAvailable || rel.TagName != "v1.6.12" {
		t.Fatalf("an Arch user on 1.6.11 is offered 1.6.12, got %v %v", sel, rel)
	}
}

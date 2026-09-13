package grpc_server

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"grpc_server/gen"
	"io"
	"net/http"
	"os"
	"path"
	"path/filepath"
	"regexp"
	"runtime"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/Masterminds/semver/v3"
	"github.com/Ogstra/proxorlib/proxor_common"
)

const (
	updateRepoOwner       = "Ogstra"
	updateRepoName        = "proxor"
	updateUserAgentPrefix = "Proxor-Updater/"
)

type githubReleaseAsset struct {
	Name               string `json:"name"`
	BrowserDownloadURL string `json:"browser_download_url"`
}

type githubRelease struct {
	TagName    string               `json:"tag_name"`
	HTMLURL    string               `json:"html_url"`
	Prerelease bool                 `json:"prerelease"`
	Body       string               `json:"body"`
	Assets     []githubReleaseAsset `json:"assets"`
}

var (
	updateDownloadURL  string
	updateAssetName    string
	updateChecksumsURL string
)

type downloadProgress struct {
	totalBytes    int64
	receivedBytes int64
	complete      bool
	err           string
}

var (
	dlProgress   downloadProgress
	dlProgressMu sync.Mutex
)

type progressWriter struct {
	w        io.Writer
	mu       *sync.Mutex
	progress *downloadProgress
}

func (pw *progressWriter) Write(p []byte) (int, error) {
	n, err := pw.w.Write(p)
	pw.mu.Lock()
	pw.progress.receivedBytes += int64(n)
	pw.mu.Unlock()
	return n, err
}

type updateSelection int

const (
	updateSelectionCurrent updateSelection = iota
	updateSelectionAvailable
	updateSelectionNoCompatible
)

type releaseVersion struct {
	major int
	minor int
	patch int
}

var releaseVersionPattern = regexp.MustCompile(`(\d+)\.(\d+)\.(\d+)`)
var semverPattern = regexp.MustCompile(`(?i)(\d+(?:\.\d+){0,2}(?:-[0-9A-Za-z.-]+)?(?:\+[0-9A-Za-z.-]+)?)`)

func updateArchiveSuffixes(goos, goarch string) ([]string, error) {
	switch {
	case goos == "windows" && goarch == "amd64":
		return []string{"windows64.zip"}, nil
	case goos == "windows" && goarch == "arm64":
		return []string{"windows-arm64.zip"}, nil
	case goos == "linux" && goarch == "amd64":
		// The AppImage is the only asset published for linux/amd64 today. This is also
		// the fallback every channel on this platform resolves to when it has no
		// channel-specific entry in suffixesForChannel (e.g. an older GUI against a
		// newer core, which sends no channel at all).
		return []string{"linux64.AppImage"}, nil
	case goos == "linux" && goarch == "arm64":
		return nil, fmt.Errorf("self-update is not available for Linux/%s", goarch)
	default:
		return nil, fmt.Errorf("self-update is not available on %s/%s", goos, goarch)
	}
}

// suffixesForChannel resolves the asset name suffix published for the channel the GUI
// detected. Knowing a version exists is independent of being able to apply it, so this
// resolves an asset even for channels the GUI will never call Download for (deb, rpm,
// arch, flatpak, winget) -- the resolved asset's file name is what the C++ guidance text
// names in the update command (e.g. "proxor_1.6.7-1_amd64.deb"). Go decides nothing about
// whether an update may be applied; that policy stays in src/main/PackagePolicy.cpp.
// An empty or unrecognised channel falls back to updateArchiveSuffixes(goos, goarch), so
// an older GUI talking to a newer core still works.
func suffixesForChannel(channel, goos, goarch string) ([]string, error) {
	if goos != "linux" {
		return updateArchiveSuffixes(goos, goarch)
	}

	switch channel {
	case "deb":
		return []string{"_amd64.deb"}, nil
	case "rpm":
		return []string{".x86_64.rpm"}, nil
	case "flatpak":
		return []string{".flatpak"}, nil
	case "winget":
		return []string{"winget-x64.zip"}, nil
	case "arch":
		// The AUR recipe builds from the source tarball, not a prebuilt binary.
		return []string{".tar.gz"}, nil
	case "appimage", "portable":
		return []string{"linux64.AppImage"}, nil
	default:
		return updateArchiveSuffixes(goos, goarch)
	}
}

func parseReleaseVersion(raw string) (releaseVersion, bool) {
	match := releaseVersionPattern.FindStringSubmatch(raw)
	if len(match) != 4 {
		return releaseVersion{}, false
	}

	major, err := strconv.Atoi(match[1])
	if err != nil {
		return releaseVersion{}, false
	}
	minor, err := strconv.Atoi(match[2])
	if err != nil {
		return releaseVersion{}, false
	}
	patch, err := strconv.Atoi(match[3])
	if err != nil {
		return releaseVersion{}, false
	}

	return releaseVersion{
		major: major,
		minor: minor,
		patch: patch,
	}, true
}

func compareReleaseVersions(left, right releaseVersion) int {
	switch {
	case left.major != right.major:
		return left.major - right.major
	case left.minor != right.minor:
		return left.minor - right.minor
	default:
		return left.patch - right.patch
	}
}

func normalizeSemVer(raw string) (string, bool) {
	match := semverPattern.FindStringSubmatch(raw)
	if len(match) != 2 {
		return "", false
	}

	version := strings.TrimSpace(match[1])
	version = strings.TrimPrefix(version, "v")
	version = strings.TrimPrefix(version, "V")
	if version == "" {
		return "", false
	}

	core := version
	suffix := ""
	if idx := strings.IndexAny(version, "-+"); idx >= 0 {
		core = version[:idx]
		suffix = version[idx:]
	}

	parts := strings.Split(core, ".")
	if len(parts) < 1 || len(parts) > 3 {
		return "", false
	}
	for len(parts) < 3 {
		parts = append(parts, "0")
	}
	for _, part := range parts {
		if part == "" {
			return "", false
		}
	}

	return strings.Join(parts, ".") + suffix, true
}

func parseSemVerVersion(raw string) (*semver.Version, bool) {
	normalized, ok := normalizeSemVer(raw)
	if !ok {
		return nil, false
	}
	version, err := semver.NewVersion(normalized)
	if err != nil {
		return nil, false
	}
	return version, true
}

func compareSemVerVersions(left, right string) (int, bool) {
	lv, ok := parseSemVerVersion(left)
	if !ok {
		return 0, false
	}
	rv, ok := parseSemVerVersion(right)
	if !ok {
		return 0, false
	}
	switch {
	case lv.Equal(rv):
		return 0, true
	case lv.GreaterThan(rv):
		return 1, true
	default:
		return -1, true
	}
}

func releaseAssetVersion(release githubRelease, asset githubReleaseAsset) (releaseVersion, bool) {
	if version, ok := parseReleaseVersion(asset.Name); ok {
		return version, true
	}
	return parseReleaseVersion(release.TagName)
}

func matchingReleaseAsset(releases []githubRelease, currentVersion string, suffixes []string, includePrerelease bool) (*githubRelease, *githubReleaseAsset, updateSelection) {
	currentParsed, hasCurrentVersion := parseSemVerVersion(currentVersion)
	var bestRelease *githubRelease
	var bestAsset *githubReleaseAsset
	var bestVersion *semver.Version
	var sawCompatibleAsset bool

	for _, release := range releases {
		if release.Prerelease && !includePrerelease {
			continue
		}
		for _, asset := range release.Assets {
			for _, suffix := range suffixes {
				if !strings.HasSuffix(asset.Name, suffix) {
					continue
				}
				sawCompatibleAsset = true

				if !hasCurrentVersion {
					if bestRelease == nil {
						releaseCopy := release
						assetCopy := asset
						bestRelease = &releaseCopy
						bestAsset = &assetCopy
					}
					continue
				}

				candidateVersion, ok := parseSemVerVersion(asset.Name)
				if !ok {
					candidateVersion, ok = parseSemVerVersion(release.TagName)
				}
				if !ok {
					continue
				}

				if currentParsed.Prerelease() != "" && !release.Prerelease {
					coreVer, err := semver.NewVersion(fmt.Sprintf("%d.%d.%d", currentParsed.Major(), currentParsed.Minor(), currentParsed.Patch()))
					if err != nil || !candidateVersion.GreaterThan(coreVer) {
						continue
					}
				} else if !candidateVersion.GreaterThan(currentParsed) {
					continue
				}
				if bestVersion == nil || candidateVersion.GreaterThan(bestVersion) {
					releaseCopy := release
					assetCopy := asset
					bestRelease = &releaseCopy
					bestAsset = &assetCopy
					bestVersion = candidateVersion
				}
			}
		}
	}

	if bestRelease != nil && bestAsset != nil {
		return bestRelease, bestAsset, updateSelectionAvailable
	}
	if hasCurrentVersion && sawCompatibleAsset {
		return nil, nil, updateSelectionCurrent
	}
	return nil, nil, updateSelectionNoCompatible
}

func downloadedArchivePath(assetName string) string {
	switch {
	case strings.HasSuffix(assetName, ".tar.gz"):
		return filepath.Join("..", "update-package.tar.gz")
	case strings.HasSuffix(assetName, ".zip"):
		return filepath.Join("..", "update-package.zip")
	default:
		ext := filepath.Ext(assetName)
		if ext == "" {
			ext = ".zip"
		}
		return filepath.Join("..", "update-package"+ext)
	}
}

// downloadDestination returns the final path Download should write assetName to. An
// empty downloadDir defers to today's behaviour, downloadedArchivePath, beside the
// install. A non-empty downloadDir is the caller's choice -- the AppImage channel is
// the reason this exists, since the core's own working directory sits inside a
// read-only FUSE mount and can never be the right answer for that channel -- and is
// validated as an existing, writable directory before it is trusted.
func downloadDestination(downloadDir, assetName string) (string, error) {
	if downloadDir == "" {
		return downloadedArchivePath(assetName), nil
	}

	info, err := os.Stat(downloadDir)
	if err != nil {
		return "", fmt.Errorf("download directory %q is not usable: %w", downloadDir, err)
	}
	if !info.IsDir() {
		return "", fmt.Errorf("download directory %q is not a directory", downloadDir)
	}

	probe, err := os.CreateTemp(downloadDir, ".proxor-update-write-check-*")
	if err != nil {
		return "", fmt.Errorf("download directory %q is not writable: %w", downloadDir, err)
	}
	probeName := probe.Name()
	probe.Close()
	os.Remove(probeName)

	return filepath.Join(downloadDir, filepath.Base(assetName)), nil
}

// checksumForAsset finds the published SHA-256 digest for assetName in sums, the raw
// contents of a release's SHA256SUMS asset. prepare-release-assets.sh generates that file
// with `shasum -a 256` from inside the output directory, which is why matching on
// path.Base (stripping both a confirmed "./" prefix and any directory component the
// wanted name might carry) is correct and matching on a full browser_download_url would
// silently never match. Fails closed: an absent or ambiguous entry is an error, never an
// empty string, because the caller must treat an unverifiable download as a failed one.
func checksumForAsset(sums, assetName string) (string, error) {
	wanted := path.Base(assetName)
	var found string

	for _, line := range strings.Split(sums, "\n") {
		line = strings.TrimRight(line, "\r")
		if strings.TrimSpace(line) == "" {
			continue
		}
		fields := strings.Fields(line)
		if len(fields) < 2 {
			continue
		}
		digest := fields[0]
		name := strings.TrimPrefix(fields[1], "*")
		if path.Base(name) != wanted {
			continue
		}
		if found != "" && !strings.EqualFold(found, digest) {
			return "", fmt.Errorf("SHA256SUMS lists %q twice with different hashes", wanted)
		}
		found = digest
	}

	if found == "" {
		return "", fmt.Errorf("SHA256SUMS has no entry for %q", wanted)
	}
	return found, nil
}

// verifyAssetChecksum is the single chokepoint a downloaded asset must pass before it is
// ever kept under its final name. A deliberately corrupted asset cannot be produced
// against a published release, so this comparison -- not a live download -- is what
// proves the mismatch path: fail closed on a missing/ambiguous SHA256SUMS entry
// (checksumForAsset) and on a digest that does not match, case-insensitively.
func verifyAssetChecksum(sums, assetName, gotDigestHex string) error {
	wantDigestHex, err := checksumForAsset(sums, assetName)
	if err != nil {
		return err
	}
	if !strings.EqualFold(wantDigestHex, gotDigestHex) {
		return fmt.Errorf("downloaded %q does not match its published SHA-256", assetName)
	}
	return nil
}

func githubError(resp *http.Response) string {
	body, _ := io.ReadAll(io.LimitReader(resp.Body, 2048))
	message := strings.TrimSpace(string(body))
	if message == "" {
		return fmt.Sprintf("GitHub API request failed with status %d", resp.StatusCode)
	}
	return fmt.Sprintf("GitHub API request failed with status %d: %s", resp.StatusCode, message)
}

func (s *BaseServer) Update(ctx context.Context, in *gen.UpdateReq) (*gen.UpdateResp, error) {
	ret := &gen.UpdateResp{}
	client := proxor_common.CreateProxyHttpClient(proxor_common.GetCurrentInstance())

	switch in.Action {
	case gen.UpdateAction_Check:
		checkCtx, cancel := context.WithTimeout(ctx, 10*time.Second)
		defer cancel()

		suffixes, err := suffixesForChannel(in.Channel, runtime.GOOS, runtime.GOARCH)
		if err != nil {
			ret.Error = err.Error()
			return ret, nil
		}

		req, err := http.NewRequestWithContext(checkCtx, http.MethodGet,
			fmt.Sprintf("https://api.github.com/repos/%s/%s/releases", updateRepoOwner, updateRepoName), nil)
		if err != nil {
			ret.Error = err.Error()
			return ret, nil
		}
		req.Header.Set("Accept", "application/vnd.github+json")
		req.Header.Set("User-Agent", updateUserAgentPrefix+proxor_common.Version_proxor)

		resp, err := client.Do(req)
		if err != nil {
			ret.Error = err.Error()
			return ret, nil
		}
		defer resp.Body.Close()
		if resp.StatusCode < 200 || resp.StatusCode >= 300 {
			ret.Error = githubError(resp)
			return ret, nil
		}

		var releases []githubRelease
		if err = json.NewDecoder(resp.Body).Decode(&releases); err != nil {
			ret.Error = err.Error()
			return ret, nil
		}
		if len(releases) == 0 {
			ret.Error = fmt.Sprintf("No releases were found in %s/%s.", updateRepoOwner, updateRepoName)
			return ret, nil
		}

		release, asset, selection := matchingReleaseAsset(releases, proxor_common.Version_proxor, suffixes, in.CheckPreRelease)
		if selection == updateSelectionCurrent {
			updateDownloadURL = ""
			updateAssetName = ""
			updateChecksumsURL = ""
			return ret, nil
		}
		if selection == updateSelectionNoCompatible || release == nil || asset == nil {
			ret.Error = fmt.Sprintf("No compatible update package was found for %s/%s in %s/%s.", runtime.GOOS, runtime.GOARCH, updateRepoOwner, updateRepoName)
			return ret, nil
		}

		updateDownloadURL = asset.BrowserDownloadURL
		// The destination is resolved later, at Download time, from whatever
		// download_dir that request names -- not here -- so a stale Check result
		// can never be reused against a directory a different request chose.
		updateAssetName = asset.Name

		// Downloads are only ever kept when they match the release's own published
		// SHA256SUMS entry for this exact asset name. An empty URL here means the
		// release published none, which Download must treat as a failure, not a
		// skip -- the asset name alone is never trusted.
		updateChecksumsURL = ""
		for _, a := range release.Assets {
			if a.Name == "SHA256SUMS" {
				updateChecksumsURL = a.BrowserDownloadURL
				break
			}
		}

		ret.AssetsName = asset.Name
		ret.DownloadUrl = asset.BrowserDownloadURL
		ret.ReleaseUrl = release.HTMLURL
		ret.ReleaseNote = release.Body
		ret.IsPreRelease = release.Prerelease
		return ret, nil

	case gen.UpdateAction_Download:
		if updateDownloadURL == "" || updateAssetName == "" {
			ret.Error = "No update package is queued for download."
			return ret, nil
		}

		destination, err := downloadDestination(in.DownloadDir, updateAssetName)
		if err != nil {
			ret.Error = err.Error()
			return ret, nil
		}
		// Write under a .part name and only rename into the final name once the
		// content is verified: a half-written file must never be visible under a
		// name the GUI will treat as a finished update.
		partPath := destination + ".part"

		req, err := http.NewRequestWithContext(ctx, http.MethodGet, updateDownloadURL, nil)
		if err != nil {
			ret.Error = err.Error()
			return ret, nil
		}
		req.Header.Set("User-Agent", updateUserAgentPrefix+proxor_common.Version_proxor)

		resp, err := client.Do(req)
		if err != nil {
			ret.Error = err.Error()
			return ret, nil
		}
		defer resp.Body.Close()
		if resp.StatusCode < 200 || resp.StatusCode >= 300 {
			ret.Error = fmt.Sprintf("Update download failed with status %d", resp.StatusCode)
			return ret, nil
		}

		file, err := os.OpenFile(partPath, os.O_TRUNC|os.O_CREATE|os.O_RDWR, 0644)
		if err != nil {
			ret.Error = err.Error()
			return ret, nil
		}
		defer file.Close()

		dlProgressMu.Lock()
		dlProgress = downloadProgress{totalBytes: resp.ContentLength}
		dlProgressMu.Unlock()

		// fail records the error on both the polled progress and the immediate
		// response, then removes the partial file: an unverifiable or incomplete
		// download is a failed download, never a kept one.
		fail := func(err error) (*gen.UpdateResp, error) {
			dlProgressMu.Lock()
			dlProgress.err = err.Error()
			dlProgressMu.Unlock()
			ret.Error = err.Error()
			os.Remove(partPath)
			return ret, nil
		}

		// The digest is computed while streaming rather than by re-reading the file,
		// so progress reporting (progressWriter) is untouched by verification.
		hasher := sha256.New()
		pw := &progressWriter{w: io.MultiWriter(file, hasher), mu: &dlProgressMu, progress: &dlProgress}
		if _, err = io.Copy(pw, resp.Body); err != nil {
			return fail(err)
		}
		if err = file.Sync(); err != nil {
			return fail(err)
		}

		if updateChecksumsURL == "" {
			return fail(fmt.Errorf("the release published no SHA256SUMS asset; refusing to apply an unverifiable download"))
		}

		sumsReq, err := http.NewRequestWithContext(ctx, http.MethodGet, updateChecksumsURL, nil)
		if err != nil {
			return fail(err)
		}
		sumsReq.Header.Set("User-Agent", updateUserAgentPrefix+proxor_common.Version_proxor)

		sumsResp, err := client.Do(sumsReq)
		if err != nil {
			return fail(err)
		}
		sumsBody, err := io.ReadAll(io.LimitReader(sumsResp.Body, 1<<20))
		sumsResp.Body.Close()
		if err != nil {
			return fail(err)
		}
		if sumsResp.StatusCode < 200 || sumsResp.StatusCode >= 300 {
			return fail(fmt.Errorf("failed to fetch SHA256SUMS: status %d", sumsResp.StatusCode))
		}

		if err = verifyAssetChecksum(string(sumsBody), updateAssetName, hex.EncodeToString(hasher.Sum(nil))); err != nil {
			return fail(err)
		}

		if err = os.Rename(partPath, destination); err != nil {
			return fail(err)
		}

		dlProgressMu.Lock()
		dlProgress.complete = true
		dlProgressMu.Unlock()

		return ret, nil

	case gen.UpdateAction_QueryProgress:
		dlProgressMu.Lock()
		snap := dlProgress
		dlProgressMu.Unlock()
		ret.ProgressTotal = snap.totalBytes
		ret.ProgressReceived = snap.receivedBytes
		ret.ProgressComplete = snap.complete
		if snap.err != "" {
			ret.Error = snap.err
		}
		return ret, nil

	default:
		ret.Error = "Unknown update action."
		return ret, nil
	}
}

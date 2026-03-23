package grpc_server

import (
	"context"
	"encoding/json"
	"fmt"
	"grpc_server/gen"
	"io"
	"net/http"
	"os"
	"path/filepath"
	"regexp"
	"runtime"
	"strconv"
	"strings"
	"time"

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
	updateDownloadURL string
	updatePackagePath string
)

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

func updateArchiveSuffixes(goos, goarch string) ([]string, error) {
	switch {
	case goos == "windows" && goarch == "amd64":
		return []string{"windows64.zip"}, nil
	case goos == "windows" && goarch == "arm64":
		return []string{"windows-arm64.zip"}, nil
	case goos == "linux" && goarch == "amd64":
		return []string{"linux64.zip", "linux64.tar.gz"}, nil
	case goos == "linux" && goarch == "arm64":
		return []string{"linux-arm64.zip", "linux-arm64.tar.gz"}, nil
	default:
		return nil, fmt.Errorf("self-update is not available on %s/%s", goos, goarch)
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

func releaseAssetVersion(release githubRelease, asset githubReleaseAsset) (releaseVersion, bool) {
	if version, ok := parseReleaseVersion(asset.Name); ok {
		return version, true
	}
	return parseReleaseVersion(release.TagName)
}

func matchingReleaseAsset(releases []githubRelease, currentVersion string, suffixes []string, includePrerelease bool) (*githubRelease, *githubReleaseAsset, updateSelection) {
	currentParsed, hasCurrentVersion := parseReleaseVersion(currentVersion)
	var bestRelease *githubRelease
	var bestAsset *githubReleaseAsset
	var bestVersion releaseVersion

	for _, release := range releases {
		if release.Prerelease && !includePrerelease {
			continue
		}
		for _, asset := range release.Assets {
			for _, suffix := range suffixes {
				if !strings.HasSuffix(asset.Name, suffix) {
					continue
				}
				if strings.Contains(asset.Name, currentVersion) || release.TagName == currentVersion {
					return nil, nil, updateSelectionCurrent
				}

				if !hasCurrentVersion {
					if bestRelease == nil {
						releaseCopy := release
						assetCopy := asset
						bestRelease = &releaseCopy
						bestAsset = &assetCopy
					}
					continue
				}

				candidateVersion, ok := releaseAssetVersion(release, asset)
				if !ok {
					continue
				}

				switch compareReleaseVersions(candidateVersion, currentParsed) {
				case 0:
					return nil, nil, updateSelectionCurrent
				case -1:
					continue
				default:
					if bestRelease == nil || compareReleaseVersions(candidateVersion, bestVersion) > 0 {
						releaseCopy := release
						assetCopy := asset
						bestRelease = &releaseCopy
						bestAsset = &assetCopy
						bestVersion = candidateVersion
					}
				}
			}
		}
	}

	if bestRelease != nil && bestAsset != nil {
		return bestRelease, bestAsset, updateSelectionAvailable
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

		suffixes, err := updateArchiveSuffixes(runtime.GOOS, runtime.GOARCH)
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
			updatePackagePath = ""
			return ret, nil
		}
		if selection == updateSelectionNoCompatible || release == nil || asset == nil {
			ret.Error = fmt.Sprintf("No compatible update package was found for %s/%s in %s/%s.", runtime.GOOS, runtime.GOARCH, updateRepoOwner, updateRepoName)
			return ret, nil
		}

		updateDownloadURL = asset.BrowserDownloadURL
		updatePackagePath = downloadedArchivePath(asset.Name)

		ret.AssetsName = asset.Name
		ret.DownloadUrl = asset.BrowserDownloadURL
		ret.ReleaseUrl = release.HTMLURL
		ret.ReleaseNote = release.Body
		ret.IsPreRelease = release.Prerelease
		return ret, nil

	case gen.UpdateAction_Download:
		if updateDownloadURL == "" || updatePackagePath == "" {
			ret.Error = "No update package is queued for download."
			return ret, nil
		}

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

		file, err := os.OpenFile(updatePackagePath, os.O_TRUNC|os.O_CREATE|os.O_RDWR, 0644)
		if err != nil {
			ret.Error = err.Error()
			return ret, nil
		}
		defer file.Close()

		if _, err = io.Copy(file, resp.Body); err != nil {
			ret.Error = err.Error()
			return ret, nil
		}
		if err = file.Sync(); err != nil {
			ret.Error = err.Error()
			return ret, nil
		}
		return ret, nil

	default:
		ret.Error = "Unknown update action."
		return ret, nil
	}
}

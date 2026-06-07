package main

import (
	"context"
	"errors"
	"fmt"
	"log"
	"os"
	"path/filepath"
	"runtime"
	"strings"

	"github.com/Masterminds/semver/v3"
	"github.com/codeclysm/extract"
)

const (
	updateExtractDir = "./update-package"
)

func ShouldUpdate(currentVersion, latestVersion string, allowPreReleases bool) (bool, error) {
	current, err := parseSemVer(currentVersion)
	if err != nil {
		return false, fmt.Errorf("invalid current version %q: %w", currentVersion, err)
	}

	latest, err := parseSemVer(latestVersion)
	if err != nil {
		return false, fmt.Errorf("invalid latest version %q: %w", latestVersion, err)
	}

	if latest.Prerelease() != "" && !allowPreReleases {
		return false, nil
	}

	return latest.GreaterThan(current), nil
}

func parseSemVer(version string) (*semver.Version, error) {
	normalized, err := normalizeSemVer(version)
	if err != nil {
		return nil, err
	}

	return semver.NewVersion(normalized)
}

func normalizeSemVer(version string) (string, error) {
	version = strings.TrimSpace(version)
	version = strings.TrimPrefix(version, "v")
	version = strings.TrimPrefix(version, "V")
	if version == "" {
		return "", errors.New("version is empty")
	}

	core := version
	suffix := ""
	if idx := strings.IndexAny(version, "-+"); idx >= 0 {
		core = version[:idx]
		suffix = version[idx:]
	}

	parts := strings.Split(core, ".")
	if len(parts) < 1 || len(parts) > 3 {
		return "", fmt.Errorf("invalid semantic version core %q", core)
	}
	for len(parts) < 3 {
		parts = append(parts, "0")
	}
	for _, part := range parts {
		if part == "" {
			return "", fmt.Errorf("invalid semantic version core %q", core)
		}
	}

	return strings.Join(parts, ".") + suffix, nil
}

func Updater() {
	updatePackagePath, err := findUpdatePackage()
	if err != nil {
		log.Fatalln(err.Error())
	}
	log.Println("updating from", updatePackagePath)

	preCleanup()
	if err := extractUpdatePackage(updatePackagePath); err != nil {
		failUpdate(err)
	}

	payloadRoot, err := findPayloadRoot(updateExtractDir)
	if err != nil {
		failUpdate(err)
	}

	removeAll("./*.dll")
	removeAll("./*.dmp")

	if err := moveReplacing(payloadRoot, "."); err != nil {
		failUpdate(fmt.Errorf("failed to install update: %w", err))
	}

	postCleanup()

}

func failUpdate(err error) {
	MessageBoxPlain("Proxor Updater", "The update could not be installed.\n\nPlease close any running Proxor processes and try again.\n\n"+err.Error())
	log.Fatalln(err.Error())
}

func preCleanup() {
	if runtime.GOOS == "linux" {
		os.RemoveAll("./usr")
	}
	os.RemoveAll(updateExtractDir)
	removeObsoleteDeploymentPaths()
}

func postCleanup() {
	os.RemoveAll(updateExtractDir)
	os.RemoveAll("./update-package.zip")
	os.RemoveAll("./update-package.tar.gz")
}

func findUpdatePackage() (string, error) {
	if len(os.Args) == 2 && Exist(os.Args[1]) {
		return os.Args[1], nil
	}

	candidates := []string{
		"./update-package.zip",
		"./update-package.tar.gz",
		"./proxor.zip",
		"./proxor.tar.gz",
	}
	for _, candidate := range candidates {
		if Exist(candidate) {
			return candidate, nil
		}
	}
	return "", errors.New("no update package was found")
}

func extractUpdatePackage(updatePackagePath string) error {
	f, err := os.Open(updatePackagePath)
	if err != nil {
		return err
	}
	defer f.Close()

	switch {
	case strings.HasSuffix(updatePackagePath, ".zip"):
		return extract.Zip(context.Background(), f, updateExtractDir, nil)
	case strings.HasSuffix(updatePackagePath, ".tar.gz"):
		return extract.Gz(context.Background(), f, updateExtractDir, nil)
	default:
		return fmt.Errorf("unsupported update package format: %s", updatePackagePath)
	}
}

func findPayloadRoot(base string) (string, error) {
	legacyRoot := filepath.Join(base, "proxor")
	if info, err := os.Stat(legacyRoot); err == nil && info.IsDir() {
		return legacyRoot, nil
	}

	entries, err := os.ReadDir(base)
	if err != nil {
		return "", err
	}

	filtered := make([]os.DirEntry, 0, len(entries))
	for _, entry := range entries {
		if entry.Name() == "__MACOSX" {
			continue
		}
		filtered = append(filtered, entry)
	}

	if len(filtered) == 1 && filtered[0].IsDir() {
		return filepath.Join(base, filtered[0].Name()), nil
	}
	if len(filtered) == 0 {
		return "", errors.New("the update package is empty")
	}
	return base, nil
}

func Exist(path string) bool {
	_, err := os.Stat(path)
	return err == nil
}

func moveReplacing(src, dst string) error {
	info, err := os.Stat(src)
	if err != nil {
		return err
	}

	if info.IsDir() {
		entries, err := os.ReadDir(src)
		if err != nil {
			return err
		}
		for _, entry := range entries {
			if err := moveReplacing(filepath.Join(src, entry.Name()), filepath.Join(dst, entry.Name())); err != nil {
				return err
			}
		}
		return os.RemoveAll(src)
	}

	if err := os.MkdirAll(filepath.Dir(dst), 0755); err != nil {
		return err
	}
	if err := os.RemoveAll(dst); err != nil {
		return err
	}
	if err := os.Rename(src, dst); err == nil {
		return nil
	}

	data, err := os.ReadFile(src)
	if err != nil {
		return err
	}
	if err := os.WriteFile(dst, data, info.Mode()); err != nil {
		return err
	}
	return os.Remove(src)
}

func removeAll(glob string) {
	files, _ := filepath.Glob(glob)
	for _, f := range files {
		os.RemoveAll(f)
	}
}

func removeObsoleteDeploymentPaths() {
	obsoletePaths := []string{
		"./geoip.dat",
		"./geoip.db",
		"./geosite.dat",
		"./geosite.db",
		"./config/plugins",
		"./generic",
		"./iconengines",
		"./imageformats",
		"./networkinformation",
		"./platforms",
		"./proxor_gui.exe",
		"./app.exe",
		"./proxor.png",
		"./runtime",
		"./config/runtime",
		"./sqldrivers",
		"./styles",
		"./tls",
		"./translations",
		"./public_res",
	}
	for _, path := range obsoletePaths {
		os.RemoveAll(path)
	}
}

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
	"time"

	"github.com/Masterminds/semver/v3"
	"github.com/codeclysm/extract"
)

const (
	updateExtractDir = "./update-package"
)

const releasesPageURL = "https://github.com/Ogstra/proxor/releases" // lists prereleases too (/latest would hide them)
const updateErrorFile = "./update-error.txt"
const headlessEnv = "PROXOR_UPDATER_HEADLESS"

const (
	replaceAttemptsWindows = 20
	replaceRetryDelay      = 250 * time.Millisecond // 20 x 250 ms = 5 s per file
)

// Test seams: single-line declarations, assigned only from _test.go files.
var hostGOOS = runtime.GOOS
var replaceRemoveAll = os.RemoveAll
var replaceRename = os.Rename
var replaceSleep = time.Sleep

func replaceAttempts(goos string) int {
	if goos == "windows" {
		return replaceAttemptsWindows
	}
	return 1
}

type failurePlan struct {
	message        string // logged; shown and written to update-error.txt on Windows
	dialog         bool   // Windows and not headless: ask to open releasesPageURL
	relaunch       bool   // Windows and !moved: start the (unchanged) ./proxor.exe again
	writeErrorFile bool   // Windows
	exitCode       int    // 0 when relaunching, else 1
}

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

// Updater applies the staged package; on failure it handles it per planAfterFailure (non-Windows: log, exit 1).
func Updater() {
	runUpdate()
}

// validatePayload checks that the extracted payload can replace the installation.
func validatePayload(root, goos string) error {
	entries, err := os.ReadDir(root)
	if err != nil {
		return fmt.Errorf("the update package is unreadable: %w", err)
	}
	if len(entries) == 0 {
		return errors.New("the update package is empty")
	}
	if goos == "windows" {
		info, err := os.Stat(filepath.Join(root, "proxor.exe"))
		if err != nil || !info.Mode().IsRegular() {
			return errors.New("the update package has no proxor.exe; refusing to install it")
		}
	}
	return nil
}

// applyUpdate applies the package without exiting, in a non-destructive order: nothing of the installation is
// deleted before the package was extracted and validated. moved reports whether the installation itself had been
// changed when err is returned.
func applyUpdate() (moved bool, err error) {
	updatePackagePath, err := findUpdatePackage()
	if err != nil {
		return false, err
	}
	log.Println("updating from", updatePackagePath)

	os.RemoveAll(updateExtractDir)
	abort := func(err error) (bool, error) {
		os.RemoveAll(updateExtractDir)
		return false, err
	}
	if err := extractUpdatePackage(updatePackagePath); err != nil {
		return abort(err)
	}
	payloadRoot, err := findPayloadRoot(updateExtractDir)
	if err != nil {
		return abort(err)
	}
	if err := validatePayload(payloadRoot, hostGOOS); err != nil {
		return abort(err)
	}

	moved = true
	if runtime.GOOS == "linux" {
		os.RemoveAll("./usr")
	}
	removeObsoleteDeploymentPaths()

	removeAll("./*.dll")
	removeAll("./*.dmp")

	if err := moveReplacing(payloadRoot, "."); err != nil {
		return moved, fmt.Errorf("failed to install update: %w", err)
	}

	postCleanup()
	return moved, nil
}

func planAfterFailure(goos string, moved bool, err error, headless bool, installDir string) failurePlan {
	if goos != "windows" {
		return failurePlan{message: err.Error(), exitCode: 1}
	}
	p := failurePlan{dialog: !headless, writeErrorFile: true}
	if moved {
		p.message = "The update could not be fully installed: " + err.Error() +
			"\n\nSome files in " + installDir + " were already replaced, so Proxor may not start correctly. Download Proxor from " +
			releasesPageURL + " and extract its proxor folder over " + installDir + " to repair it."
		p.exitCode = 1
		return p
	}
	p.message = "The update could not be installed: " + err.Error() +
		"\n\nNothing was changed: the installed Proxor keeps working and starts again now.\n\nYou can download the new version manually from " +
		releasesPageURL
	p.relaunch = true
	return p
}

// runUpdate applies the package and handles a failure per planAfterFailure. It returns true when the caller should
// start Proxor; otherwise it exits the process after logging.
func runUpdate() bool {
	moved, err := applyUpdate()
	if err == nil {
		if hostGOOS == "windows" {
			_ = os.Remove(updateErrorFile)
		}
		return true
	}
	wd, _ := os.Getwd()
	p := planAfterFailure(hostGOOS, moved, err, os.Getenv(headlessEnv) == "1", wd)
	log.Println(p.message)
	if p.writeErrorFile {
		_ = os.WriteFile(updateErrorFile, []byte(p.message+"\n"), 0644)
	}
	if p.dialog && MessageBoxYesNo("Proxor Updater", p.message+"\n\nOpen the download page now?") {
		openURL(releasesPageURL)
	}
	if !p.relaunch {
		os.Exit(p.exitCode)
	}
	return true
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

	attempts := replaceAttempts(hostGOOS)
	var lastErr error
	for i := 0; i < attempts; i++ {
		if i > 0 {
			replaceSleep(replaceRetryDelay)
		}
		lastErr = replaceFile(src, dst, info)
		if lastErr == nil {
			return nil
		}
	}
	return fmt.Errorf("could not replace %s: %w", dst, lastErr)
}

// replaceFile makes one attempt to put src in place of dst.
func replaceFile(src, dst string, info os.FileInfo) error {
	if err := replaceRemoveAll(dst); err != nil {
		return err
	}
	if err := replaceRename(src, dst); err == nil {
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

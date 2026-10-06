package grpc_server

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"grpc_server/gen"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"sync"
	"testing"
	"time"

	"github.com/Ogstra/proxorlib/proxor_common"
)

type fakeAsset struct {
	name string
	body []byte
}

type fakeReleaseServer struct {
	URL string

	tag         string
	mu          sync.Mutex
	assets      map[string][]byte
	order       []string
	sums        *string
	assetStatus int
	slowAsset   time.Duration
	paths       []string
	userAgents  []string
}

func sha256Hex(b []byte) string {
	sum := sha256.Sum256(b)
	return hex.EncodeToString(sum[:])
}

// newFakeReleaseServer serves a one-release GitHub API and its assets from a local httptest server.
func newFakeReleaseServer(t *testing.T, tag string, assets ...fakeAsset) *fakeReleaseServer {
	t.Helper()
	f := &fakeReleaseServer{tag: tag, assets: map[string][]byte{}}
	for _, a := range assets {
		f.assets[a.name] = a.body
		f.order = append(f.order, a.name)
	}
	srv := httptest.NewServer(http.HandlerFunc(f.handle))
	t.Cleanup(srv.Close)
	f.URL = srv.URL
	return f
}

func (f *fakeReleaseServer) releasePage() string {
	return f.URL + "/Ogstra/proxor/releases/tag/" + f.tag
}

func (f *fakeReleaseServer) setSums(body string) {
	f.mu.Lock()
	defer f.mu.Unlock()
	f.sums = &body
}

func (f *fakeReleaseServer) setAssetStatus(code int) {
	f.mu.Lock()
	defer f.mu.Unlock()
	f.assetStatus = code
}

func (f *fakeReleaseServer) setSlowAsset(d time.Duration) {
	f.mu.Lock()
	defer f.mu.Unlock()
	f.slowAsset = d
}

func (f *fakeReleaseServer) requests() (paths, userAgents []string) {
	f.mu.Lock()
	defer f.mu.Unlock()
	return append([]string(nil), f.paths...), append([]string(nil), f.userAgents...)
}

func (f *fakeReleaseServer) sumsBody() string {
	if f.sums != nil {
		return *f.sums
	}
	var b strings.Builder
	for _, name := range f.order {
		fmt.Fprintf(&b, "%s  ./%s\n", sha256Hex(f.assets[name]), name)
	}
	return b.String()
}

func (f *fakeReleaseServer) handle(w http.ResponseWriter, r *http.Request) {
	f.mu.Lock()
	f.paths = append(f.paths, r.URL.Path)
	f.userAgents = append(f.userAgents, r.Header.Get("User-Agent"))
	status, slow, sums := f.assetStatus, f.slowAsset, f.sumsBody()
	f.mu.Unlock()

	switch {
	case r.Method == http.MethodGet && r.URL.Path == "/repos/Ogstra/proxor/releases":
		type asset struct {
			Name string `json:"name"`
			URL  string `json:"browser_download_url"`
		}
		var list []asset
		for _, name := range f.order {
			list = append(list, asset{name, f.URL + "/download/" + name})
		}
		list = append(list, asset{"SHA256SUMS", f.URL + "/download/SHA256SUMS"})
		release := map[string]interface{}{
			"tag_name":   f.tag,
			"html_url":   f.releasePage(),
			"prerelease": false,
			"body":       "fake release",
			"assets":     list,
		}
		w.Header().Set("Content-Type", "application/json")
		_ = json.NewEncoder(w).Encode([]interface{}{release})
	case r.Method == http.MethodGet && r.URL.Path == "/download/SHA256SUMS":
		_, _ = w.Write([]byte(sums))
	case r.Method == http.MethodGet && strings.HasPrefix(r.URL.Path, "/download/"):
		body, ok := f.assets[strings.TrimPrefix(r.URL.Path, "/download/")]
		if !ok {
			http.NotFound(w, r)
			return
		}
		if status != 0 && status != http.StatusOK {
			http.Error(w, "asset unavailable", status)
			return
		}
		w.Header().Set("Content-Length", fmt.Sprint(len(body)))
		if slow <= 0 {
			_, _ = w.Write(body)
			return
		}
		half := len(body) / 2
		_, _ = w.Write(body[:half])
		if fl, ok := w.(http.Flusher); ok {
			fl.Flush()
		}
		time.Sleep(slow)
		_, _ = w.Write(body[half:])
	default:
		http.NotFound(w, r)
	}
}

// useFakeRelease points the update code at f and restores every global it touches on cleanup.
func useFakeRelease(t *testing.T, f *fakeReleaseServer, goos, goarch, currentVersion string) {
	t.Helper()
	oldBase, oldOS, oldArch := updateAPIBaseURL, updateGOOS, updateGOARCH
	oldVer := proxor_common.Version_proxor
	oldClient, oldInst := proxor_common.CreateProxyHttpClient, proxor_common.GetCurrentInstance
	oldURL, oldName, oldSums := updateDownloadURL, updateAssetName, updateChecksumsURL
	t.Cleanup(func() {
		updateAPIBaseURL, updateGOOS, updateGOARCH = oldBase, oldOS, oldArch
		proxor_common.Version_proxor = oldVer
		proxor_common.CreateProxyHttpClient, proxor_common.GetCurrentInstance = oldClient, oldInst
		updateDownloadURL, updateAssetName, updateChecksumsURL = oldURL, oldName, oldSums
		dlProgressMu.Lock()
		dlProgress = downloadProgress{}
		dlProgressMu.Unlock()
	})
	updateAPIBaseURL, updateGOOS, updateGOARCH = f.URL, goos, goarch
	proxor_common.Version_proxor = currentVersion
	proxor_common.GetCurrentInstance = func() interface{} { return nil }
	proxor_common.CreateProxyHttpClient = func(interface{}) *http.Client {
		return &http.Client{Timeout: 2 * time.Minute}
	}
	updateDownloadURL, updateAssetName, updateChecksumsURL = "", "", ""
	dlProgressMu.Lock()
	dlProgress = downloadProgress{}
	dlProgressMu.Unlock()
}

// stageInstall creates <tmp>/install/config, makes it the working directory (the core's cwd in production)
// and returns <tmp>/install.
func stageInstall(t *testing.T) string {
	t.Helper()
	install := filepath.Join(t.TempDir(), "install")
	if err := os.MkdirAll(filepath.Join(install, "config"), 0o755); err != nil {
		t.Fatal(err)
	}
	t.Chdir(filepath.Join(install, "config"))
	return install
}

func callUpdate(t *testing.T, action gen.UpdateAction) *gen.UpdateResp {
	t.Helper()
	ret, err := (&BaseServer{}).Update(context.Background(), &gen.UpdateReq{Action: action})
	if err != nil {
		t.Fatalf("Update(%v) returned a Go error: %v", action, err)
	}
	return ret
}

// stubRetrySleep records the waits instead of sleeping.
func stubRetrySleep(t *testing.T) *[]time.Duration {
	t.Helper()
	var waits []time.Duration
	old := retrySleep
	retrySleep = func(d time.Duration) { waits = append(waits, d) }
	t.Cleanup(func() { retrySleep = old })
	return &waits
}

// partFiles lists leftover update-package.zip*.part names in dir (no filepath.Glob).
func partFiles(t *testing.T, dir string) []string {
	t.Helper()
	entries, err := os.ReadDir(dir)
	if err != nil {
		t.Fatal(err)
	}
	var out []string
	for _, e := range entries {
		n := e.Name()
		if strings.HasPrefix(n, "update-package.zip") && strings.HasSuffix(n, ".part") {
			out = append(out, n)
		}
	}
	sort.Strings(out)
	return out
}

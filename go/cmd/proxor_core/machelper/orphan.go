package machelper

import (
	"io/fs"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"sync"
	"time"
)

const (
	// orphanMisses is how many spaced "app is gone" observations trigger removal.
	orphanMisses = 3
	// orphanSpacing is the minimum gap between two counted misses. It covers
	// the window in which `brew upgrade` swaps the app bundle.
	orphanSpacing = 10 * time.Minute
)

// OrphanTracker decides when the helper is orphaned (the app it was installed
// for is gone). It is pure: the caller supplies the clock and the observation.
// Not safe for concurrent use; the daemon drives it from one goroutine.
type OrphanTracker struct {
	misses int
	last   time.Time // time of the last counted miss
}

// Misses returns the number of counted consecutive misses.
func (t *OrphanTracker) Misses() int { return t.misses }

// Observe records one observation and reports whether the helper should remove
// itself now: after orphanMisses consecutive misses, each at least orphanSpacing
// after the previous counted one. A miss that comes sooner is ignored (neither
// counted nor resetting); a hit resets the count.
func (t *OrphanTracker) Observe(now time.Time, exists bool) bool {
	if exists {
		t.misses = 0
		t.last = time.Time{}
		return false
	}
	if t.misses == 0 || now.Sub(t.last) >= orphanSpacing {
		t.misses++
		t.last = now
	}
	return t.misses >= orphanMisses
}

// ObserveApp observes the recorded app path. An empty or relative path (an
// unreadable or damaged app-path file) never triggers removal: when in doubt
// the helper stays.
func (t *OrphanTracker) ObserveApp(now time.Time, path string, exists func(string) bool) bool {
	path = strings.TrimSpace(path)
	if path == "" || !filepath.IsAbs(path) {
		t.misses = 0
		t.last = time.Time{}
		return false
	}
	return t.Observe(now, exists(path))
}

// ParseAllowedUIDs parses the allowed-uids file: one decimal uid per line.
// Junk lines and uid 0 are dropped (root is always allowed by the server).
func ParseAllowedUIDs(s string) map[uint32]bool {
	out := map[uint32]bool{}
	for _, line := range strings.Split(s, "\n") {
		line = strings.TrimSpace(line)
		if line == "" {
			continue
		}
		n, err := strconv.ParseUint(line, 10, 32)
		if err != nil || n == 0 {
			continue
		}
		out[uint32(n)] = true
	}
	return out
}

// fileAllowlist serves the uid allowlist from a root-owned file and re-reads it
// whenever its mtime, size or mode changes (a second user's install appends a
// uid while the daemon runs). It fails closed: a missing, unreadable or
// group/world-writable file allows nobody (root is allowed by the server).
type fileAllowlist struct {
	path string

	mu    sync.Mutex
	valid bool
	mtime time.Time
	size  int64
	mode  fs.FileMode
	set   map[uint32]bool
}

// Allowed reports whether uid is on the allowlist.
func (a *fileAllowlist) Allowed(uid uint32) bool {
	a.mu.Lock()
	defer a.mu.Unlock()
	a.refreshLocked()
	return a.set[uid]
}

func (a *fileAllowlist) refreshLocked() {
	fi, err := os.Stat(a.path)
	if err != nil || !fi.Mode().IsRegular() {
		a.valid, a.set = false, nil
		return
	}
	if a.valid && fi.ModTime().Equal(a.mtime) && fi.Size() == a.size && fi.Mode() == a.mode {
		return
	}
	a.valid, a.mtime, a.size, a.mode = true, fi.ModTime(), fi.Size(), fi.Mode()
	a.set = nil
	if fi.Mode().Perm()&0o022 != 0 {
		return // somebody other than the owner can write it: not trusted
	}
	data, err := os.ReadFile(a.path)
	if err != nil {
		return
	}
	a.set = ParseAllowedUIDs(string(data))
}

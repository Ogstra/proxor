package machelper

import (
	"reflect"
	"testing"
	"time"
)

func TestOrphanTracker(t *testing.T) {
	t0 := time.Date(2026, 1, 1, 12, 0, 0, 0, time.UTC)
	min := func(n int) time.Time { return t0.Add(time.Duration(n) * time.Minute) }

	t.Run("three spaced misses remove", func(t *testing.T) {
		var tr OrphanTracker
		if tr.Observe(min(0), false) {
			t.Fatal("first miss must not remove")
		}
		if tr.Observe(min(11), false) {
			t.Fatal("second miss must not remove")
		}
		if !tr.Observe(min(22), false) {
			t.Fatal("third spaced miss must remove")
		}
	})

	t.Run("exactly ten minutes apart counts", func(t *testing.T) {
		var tr OrphanTracker
		tr.Observe(min(0), false)
		tr.Observe(min(10), false)
		if !tr.Observe(min(20), false) {
			t.Fatal("misses exactly 10 min apart must count")
		}
	})

	t.Run("close misses are not counted", func(t *testing.T) {
		var tr OrphanTracker
		tr.Observe(min(0), false)
		tr.Observe(min(1), false)
		tr.Observe(min(2), false)
		tr.Observe(min(3), false)
		if tr.Observe(min(9), false) {
			t.Fatal("misses closer than 10 min must not accumulate")
		}
		if tr.Misses() != 1 {
			t.Fatalf("Misses = %d, want 1", tr.Misses())
		}
	})

	t.Run("present resets", func(t *testing.T) {
		var tr OrphanTracker
		tr.Observe(min(0), false)
		tr.Observe(min(11), false)
		if tr.Observe(min(22), true) {
			t.Fatal("an existing app must never remove")
		}
		if tr.Observe(min(33), false) || tr.Observe(min(44), false) {
			t.Fatal("count must restart after the app reappeared")
		}
		if !tr.Observe(min(55), false) {
			t.Fatal("third miss after the reset must remove")
		}
	})

	t.Run("stays true while still missing", func(t *testing.T) {
		var tr OrphanTracker
		tr.Observe(min(0), false)
		tr.Observe(min(11), false)
		tr.Observe(min(22), false)
		if !tr.Observe(min(33), false) {
			t.Fatal("once due, keeps saying remove (caller may defer while busy)")
		}
	})
}

func TestOrphanTrackerEmptyPathFailsSafe(t *testing.T) {
	t0 := time.Date(2026, 1, 1, 12, 0, 0, 0, time.UTC)
	var tr OrphanTracker
	exists := func(string) bool { return false }
	for i := 0; i < 10; i++ {
		if tr.ObserveApp(t0.Add(time.Duration(i*11)*time.Minute), "", exists) {
			t.Fatal("empty recorded path must never trigger removal")
		}
	}
	if tr.Misses() != 0 {
		t.Fatalf("Misses = %d, want 0", tr.Misses())
	}

	// A relative path is as untrustworthy as an empty one.
	if tr.ObserveApp(t0.Add(200*time.Minute), "Proxor.app", exists) {
		t.Fatal("relative path must not trigger removal")
	}
}

func TestOrphanTrackerObserveApp(t *testing.T) {
	t0 := time.Date(2026, 1, 1, 12, 0, 0, 0, time.UTC)
	var tr OrphanTracker
	var seen []string
	exists := func(p string) bool { seen = append(seen, p); return false }
	tr.ObserveApp(t0, "/Applications/Proxor.app", exists)
	tr.ObserveApp(t0.Add(11*time.Minute), "/Applications/Proxor.app", exists)
	if !tr.ObserveApp(t0.Add(22*time.Minute), "/Applications/Proxor.app", exists) {
		t.Fatal("three misses on an absolute path must remove")
	}
	if len(seen) != 3 || seen[0] != "/Applications/Proxor.app" {
		t.Fatalf("exists called with %v", seen)
	}
}

func TestParseAllowedUIDs(t *testing.T) {
	got := ParseAllowedUIDs("501\n502\n\nabc\n0\n")
	want := map[uint32]bool{501: true, 502: true}
	if !reflect.DeepEqual(got, want) {
		t.Fatalf("got %v, want %v", got, want)
	}
	if got := ParseAllowedUIDs(""); len(got) != 0 {
		t.Fatalf("empty input gave %v", got)
	}
	got = ParseAllowedUIDs(" 503 \r\n-1\n99999999999\n504")
	want = map[uint32]bool{503: true, 504: true}
	if !reflect.DeepEqual(got, want) {
		t.Fatalf("got %v, want %v", got, want)
	}
}

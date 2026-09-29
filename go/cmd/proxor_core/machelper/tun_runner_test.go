package machelper

import (
	"context"
	"errors"
	"io"
	"strings"
	"sync"
	"testing"
	"time"

	"github.com/sagernet/sing-box/option"
)

// tunFake records every hook call so the tests can prove ordering (validate,
// check, wait for SOCKS, create) without touching the OS.
type tunFake struct {
	mu           sync.Mutex
	checks       int
	creates      int
	dials        int
	dialsAtMake  int
	dialFailures int // fail this many dials, then succeed; <0: always fail
	checkErr     error
	createErr    error
	closes       int
	cancels      int
	writer       io.Writer
	writerSets   int
}

type fakeCloser struct{ f *tunFake }

func (c fakeCloser) Close() error {
	c.f.mu.Lock()
	defer c.f.mu.Unlock()
	c.f.closes++
	return nil
}

func (f *tunFake) hooks() tunHooks {
	return tunHooks{
		check: func(ctx context.Context, o option.Options) error {
			f.mu.Lock()
			defer f.mu.Unlock()
			f.checks++
			return f.checkErr
		},
		create: func(ctx context.Context, o option.Options) (io.Closer, context.CancelFunc, error) {
			f.mu.Lock()
			defer f.mu.Unlock()
			f.creates++
			f.dialsAtMake = f.dials
			if f.createErr != nil {
				return nil, nil, f.createErr
			}
			return fakeCloser{f}, func() {
				f.mu.Lock()
				f.cancels++
				f.mu.Unlock()
			}, nil
		},
		dial: func(addr string, timeout time.Duration) error {
			f.mu.Lock()
			defer f.mu.Unlock()
			f.dials++
			if f.dialFailures < 0 || f.dials <= f.dialFailures {
				return errors.New("connection refused")
			}
			return nil
		},
		setLogWriter: func(w io.Writer) {
			f.mu.Lock()
			defer f.mu.Unlock()
			f.writer = w
			f.writerSets++
		},
	}
}

func (f *tunFake) get(fn func(f *tunFake) int) int {
	f.mu.Lock()
	defer f.mu.Unlock()
	return fn(f)
}

func newTestRunner(f *tunFake) *BoxTunRunner {
	r := NewBoxTunRunner()
	r.hooks = f.hooks()
	r.poll = time.Millisecond
	return r
}

type eventSink struct{ ch chan Event }

func newSink() *eventSink { return &eventSink{ch: make(chan Event, 64)} }

func (s *eventSink) emit(ev Event) { s.ch <- ev }

func (s *eventSink) next(t *testing.T) Event {
	t.Helper()
	select {
	case ev := <-s.ch:
		return ev
	case <-time.After(3 * time.Second):
		t.Fatal("timed out waiting for an event")
		return Event{}
	}
}

func (s *eventSink) expectNone(t *testing.T) {
	t.Helper()
	select {
	case ev := <-s.ch:
		t.Fatalf("unexpected event %+v", ev)
	case <-time.After(80 * time.Millisecond):
	}
}

func validTunConfig(t *testing.T) []byte {
	t.Helper()
	return renderTemplate(t, renderOpts{})
}

func TestTunRunnerRejectsInvalid(t *testing.T) {
	f := &tunFake{}
	r := newTestRunner(f)
	sink := newSink()
	if err := r.Start([]byte(`{"outbounds":[{"type":"direct","tag":"x"}]}`), 2080, sink.emit); err == nil {
		t.Fatal("invalid config accepted")
	}
	if err := r.Start(validTunConfig(t), 0, sink.emit); err == nil {
		t.Fatal("invalid socks port accepted")
	}
	if n := f.get(func(f *tunFake) int { return f.checks + f.creates + f.dials }); n != 0 {
		t.Fatalf("nothing may run for an invalid config, hooks called %d times", n)
	}
	if r.Running() {
		t.Fatal("Running() after rejected start")
	}
	sink.expectNone(t)
}

func TestTunRunnerCheckError(t *testing.T) {
	f := &tunFake{checkErr: errors.New("sing-box check says no")}
	r := newTestRunner(f)
	sink := newSink()
	err := r.Start(validTunConfig(t), 2080, sink.emit)
	if err == nil || !strings.Contains(err.Error(), "sing-box check says no") {
		t.Fatalf("want the check error, got %v", err)
	}
	if f.get(func(f *tunFake) int { return f.creates + f.dials }) != 0 || r.Running() {
		t.Fatal("nothing may start after a failed check")
	}
	sink.expectNone(t)
}

func TestTunRunnerWaitsForSocks(t *testing.T) {
	f := &tunFake{dialFailures: 3}
	r := newTestRunner(f)
	sink := newSink()
	if err := r.Start(validTunConfig(t), 2080, sink.emit); err != nil {
		t.Fatal(err)
	}
	if ev := sink.next(t); ev.Event != EventTunReady {
		t.Fatalf("want tun_ready, got %+v", ev)
	}
	if got := f.get(func(f *tunFake) int { return f.dialsAtMake }); got != 4 {
		t.Fatalf("create must run right after the 4th dial, dials at create = %d", got)
	}
	if got := f.get(func(f *tunFake) int { return f.creates }); got != 1 {
		t.Fatalf("creates = %d", got)
	}
	if !r.Running() {
		t.Fatal("Running() false after tun_ready")
	}
	sink.expectNone(t)
	if err := r.Stop(); err != nil {
		t.Fatal(err)
	}
}

func TestTunRunnerSocksTimeout(t *testing.T) {
	f := &tunFake{dialFailures: -1}
	r := newTestRunner(f)
	r.SocksWait = 300 * time.Millisecond
	sink := newSink()
	if err := r.Start(validTunConfig(t), 2080, sink.emit); err != nil {
		t.Fatal(err)
	}
	ev := sink.next(t)
	if ev.Event != EventTunStopped || !strings.Contains(ev.Reason, "SOCKS port 2080") {
		t.Fatalf("want tun_stopped naming the SOCKS port, got %+v", ev)
	}
	if f.get(func(f *tunFake) int { return f.creates }) != 0 {
		t.Fatal("create must not run when the core never came up")
	}
	if r.Running() {
		t.Fatal("Running() after timeout")
	}
}

func TestTunRunnerCreateError(t *testing.T) {
	f := &tunFake{createErr: errors.New("utun busy")}
	r := newTestRunner(f)
	sink := newSink()
	if err := r.Start(validTunConfig(t), 2080, sink.emit); err != nil {
		t.Fatal(err)
	}
	ev := sink.next(t)
	if ev.Event != EventTunStopped || !strings.Contains(ev.Reason, "utun busy") {
		t.Fatalf("want tun_stopped with the create error, got %+v", ev)
	}
	if r.Running() {
		t.Fatal("Running() after failed create")
	}
	f.mu.Lock()
	defer f.mu.Unlock()
	if f.writer != nil {
		t.Fatal("log writer must be cleared after a failed create")
	}
}

func TestTunRunnerStopDuringWait(t *testing.T) {
	f := &tunFake{dialFailures: -1}
	r := newTestRunner(f)
	sink := newSink()
	if err := r.Start(validTunConfig(t), 2080, sink.emit); err != nil {
		t.Fatal(err)
	}
	deadline := time.Now().Add(2 * time.Second)
	for f.get(func(f *tunFake) int { return f.dials }) == 0 {
		if time.Now().After(deadline) {
			t.Fatal("runner never polled the SOCKS port")
		}
		time.Sleep(time.Millisecond)
	}
	if err := r.Stop(); err != nil {
		t.Fatal(err)
	}
	if r.Running() {
		t.Fatal("Running() after Stop")
	}
	sink.expectNone(t)
	if f.get(func(f *tunFake) int { return f.creates }) != 0 {
		t.Fatal("create ran after Stop")
	}

	// A stopped runner can be started again.
	f.mu.Lock()
	f.dialFailures = 0
	f.mu.Unlock()
	if err := r.Start(validTunConfig(t), 2080, sink.emit); err != nil {
		t.Fatal(err)
	}
	if ev := sink.next(t); ev.Event != EventTunReady {
		t.Fatalf("want tun_ready after restart, got %+v", ev)
	}
	_ = r.Stop()
}

func TestTunRunnerStopIsIdempotentAndCleansUp(t *testing.T) {
	f := &tunFake{}
	r := newTestRunner(f)
	if err := r.Stop(); err != nil {
		t.Fatalf("Stop on a fresh runner: %v", err)
	}
	sink := newSink()
	if err := r.Start(validTunConfig(t), 2080, sink.emit); err != nil {
		t.Fatal(err)
	}
	sink.next(t)
	for i := 0; i < 3; i++ {
		if err := r.Stop(); err != nil {
			t.Fatalf("Stop #%d: %v", i, err)
		}
	}
	f.mu.Lock()
	defer f.mu.Unlock()
	if f.closes != 1 || f.cancels != 1 {
		t.Fatalf("box must be closed and cancelled exactly once, closes=%d cancels=%d", f.closes, f.cancels)
	}
	if f.writer != nil {
		t.Fatal("log writer not cleared by Stop")
	}
	if r.Running() {
		t.Fatal("Running() after Stop")
	}
}

func TestTunRunnerRestartClosesPrevious(t *testing.T) {
	f := &tunFake{}
	r := newTestRunner(f)
	sink := newSink()
	if err := r.Start(validTunConfig(t), 2080, sink.emit); err != nil {
		t.Fatal(err)
	}
	sink.next(t)
	sink2 := newSink()
	if err := r.Start(validTunConfig(t), 2080, sink2.emit); err != nil {
		t.Fatal(err)
	}
	if ev := sink2.next(t); ev.Event != EventTunReady {
		t.Fatalf("want tun_ready, got %+v", ev)
	}
	if got := f.get(func(f *tunFake) int { return f.closes }); got != 1 {
		t.Fatalf("previous instance must be closed once, closes=%d", got)
	}
	sink.expectNone(t) // the superseded start reports nothing more
	_ = r.Stop()
}

func TestTunRunnerLogForwarding(t *testing.T) {
	f := &tunFake{}
	r := newTestRunner(f)
	sink := newSink()
	if err := r.Start(validTunConfig(t), 2080, sink.emit); err != nil {
		t.Fatal(err)
	}
	sink.next(t) // tun_ready
	f.mu.Lock()
	w := f.writer
	f.mu.Unlock()
	if w == nil {
		t.Fatal("no log writer installed")
	}

	io.WriteString(w, "line\n")
	if ev := sink.next(t); ev.Event != EventLog || ev.Line != "line" {
		t.Fatalf("want log line, got %+v", ev)
	}

	// Several lines in one write, empty lines dropped, a line split across writes.
	io.WriteString(w, "a\nb\n\npar")
	io.WriteString(w, "tial\n")
	for _, want := range []string{"a", "b", "partial"} {
		if ev := sink.next(t); ev.Event != EventLog || ev.Line != want {
			t.Fatalf("want log %q, got %+v", want, ev)
		}
	}

	// Over-long lines are capped, and the rest of the line is dropped.
	io.WriteString(w, strings.Repeat("x", 10000)+"\nnext\n")
	ev := sink.next(t)
	if ev.Event != EventLog || len(ev.Line) != maxLogLine {
		t.Fatalf("long line not capped to %d: len=%d", maxLogLine, len(ev.Line))
	}
	if ev := sink.next(t); ev.Line != "next" {
		t.Fatalf("line after a capped one lost: %+v", ev)
	}

	// Late writes from a stopped instance are dropped.
	_ = r.Stop()
	io.WriteString(w, "late\n")
	sink.expectNone(t)
}

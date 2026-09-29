package machelper

import (
	"bufio"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net"
	"os"
	"path/filepath"
	"reflect"
	"strings"
	"sync"
	"sync/atomic"
	"testing"
	"time"
)

// ---- fakes ----------------------------------------------------------------

type callLog struct {
	mu    sync.Mutex
	calls []string
}

func (l *callLog) add(s string) {
	l.mu.Lock()
	l.calls = append(l.calls, s)
	l.mu.Unlock()
}

func (l *callLog) snapshot() []string {
	l.mu.Lock()
	defer l.mu.Unlock()
	return append([]string(nil), l.calls...)
}

func (l *callLog) count(prefix string) int {
	n := 0
	for _, c := range l.snapshot() {
		if strings.HasPrefix(c, prefix) {
			n++
		}
	}
	return n
}

type fakeTun struct {
	mu       sync.Mutex
	log      *callLog
	startErr error
	running  bool
	emits    []func(Event)
	configs  []string
	ports    []int
}

func (f *fakeTun) Start(config []byte, socksPort int, emit func(Event)) error {
	f.mu.Lock()
	defer f.mu.Unlock()
	f.log.add("tun.start")
	if f.startErr != nil {
		return f.startErr
	}
	f.running = true
	f.emits = append(f.emits, emit)
	f.configs = append(f.configs, string(config))
	f.ports = append(f.ports, socksPort)
	return nil
}

func (f *fakeTun) Stop() error {
	f.mu.Lock()
	defer f.mu.Unlock()
	f.log.add("tun.stop")
	f.running = false
	return nil
}

func (f *fakeTun) Running() bool {
	f.mu.Lock()
	defer f.mu.Unlock()
	return f.running
}

func (f *fakeTun) setStartErr(err error) {
	f.mu.Lock()
	f.startErr = err
	f.mu.Unlock()
}

func (f *fakeTun) lastStart() (string, int) {
	f.mu.Lock()
	defer f.mu.Unlock()
	return f.configs[len(f.configs)-1], f.ports[len(f.ports)-1]
}

// emitter returns the emit func handed to the n-th successful Start.
func (f *fakeTun) emitter(n int) func(Event) {
	f.mu.Lock()
	defer f.mu.Unlock()
	return f.emits[n]
}

type applyCall struct {
	port   int
	bypass []string
}

type fakeProxy struct {
	mu         sync.Mutex
	log        *callLog
	applied    bool
	applyCalls []applyCall
	applyErr   error
	restoreErr error
}

func (f *fakeProxy) Apply(port int, bypass []string) ([]string, []string, error) {
	f.mu.Lock()
	defer f.mu.Unlock()
	f.log.add("proxy.apply")
	f.applyCalls = append(f.applyCalls, applyCall{port, append([]string(nil), bypass...)})
	if f.applyErr != nil {
		return nil, nil, f.applyErr
	}
	f.applied = true
	return []string{"Wi-Fi"}, []string{"Thunderbolt: boom"}, nil
}

func (f *fakeProxy) Restore() error {
	f.mu.Lock()
	defer f.mu.Unlock()
	f.log.add("proxy.restore")
	if f.restoreErr != nil {
		return f.restoreErr
	}
	f.applied = false
	return nil
}

func (f *fakeProxy) Applied() bool {
	f.mu.Lock()
	defer f.mu.Unlock()
	return f.applied
}

func (f *fakeProxy) setErrors(apply, restore error) {
	f.mu.Lock()
	f.applyErr, f.restoreErr = apply, restore
	f.mu.Unlock()
}

func (f *fakeProxy) applyCallsCopy() []applyCall {
	f.mu.Lock()
	defer f.mu.Unlock()
	return append([]applyCall(nil), f.applyCalls...)
}

// ---- harness --------------------------------------------------------------

type harness struct {
	t      *testing.T
	srv    *Server
	tun    *fakeTun
	proxy  *fakeProxy
	log    *callLog
	sock   string
	nextID atomic.Uint32 // uid reported by the injected PeerUID for the next accept
	logMu  sync.Mutex
	logs   []string
}

const testUID = 501

func newHarness(t *testing.T, mut func(h *harness)) *harness {
	t.Helper()
	h := &harness{t: t, log: &callLog{}}
	h.tun = &fakeTun{log: h.log}
	h.proxy = &fakeProxy{log: h.log}
	h.nextID.Store(testUID)
	h.srv = &Server{
		Tun:   h.tun,
		Proxy: h.proxy,
		PeerUID: func(net.Conn) (uint32, error) {
			return h.nextID.Load(), nil
		},
		Allowed: func(uid uint32) bool { return uid == testUID },
		Build:   "1.6.12",
		SingBox: "1.13.13",
		Logf: func(format string, args ...any) {
			h.logMu.Lock()
			h.logs = append(h.logs, fmt.Sprintf(format, args...))
			h.logMu.Unlock()
		},
	}
	if mut != nil {
		mut(h)
	}
	// macOS sun_path is limited to 104 bytes; t.TempDir() is too long there.
	dir, err := os.MkdirTemp("", "mh")
	if err != nil {
		t.Fatal(err)
	}
	h.sock = filepath.Join(dir, "s")
	l, err := net.Listen("unix", h.sock)
	if err != nil {
		os.RemoveAll(dir)
		t.Fatal(err)
	}
	done := make(chan struct{})
	go func() {
		defer close(done)
		_ = h.srv.Serve(l)
	}()
	t.Cleanup(func() {
		l.Close()
		select {
		case <-done:
		case <-time.After(2 * time.Second):
			t.Error("Serve did not return after the listener was closed")
		}
		h.srv.Shutdown()
		os.RemoveAll(dir)
	})
	return h
}

func (h *harness) logged() string {
	h.logMu.Lock()
	defer h.logMu.Unlock()
	return strings.Join(h.logs, "\n")
}

type testClient struct {
	t  *testing.T
	c  net.Conn
	br *bufio.Reader
}

func (h *harness) dial() *testClient {
	h.t.Helper()
	c, err := net.Dial("unix", h.sock)
	if err != nil {
		h.t.Fatal(err)
	}
	h.t.Cleanup(func() { c.Close() })
	return &testClient{t: h.t, c: c, br: bufio.NewReader(c)}
}

// dialAs connects with the given uid and completes the hello handshake.
func (h *harness) dialAs(uid uint32) *testClient {
	h.t.Helper()
	h.nextID.Store(uid)
	c := h.dial()
	c.send(`{"id":1,"cmd":"hello","protocol":1}`)
	resp := c.recv()
	if resp["ok"] != true {
		h.t.Fatalf("hello failed: %v", resp)
	}
	return c
}

func (c *testClient) send(line string) {
	c.t.Helper()
	c.c.SetWriteDeadline(time.Now().Add(2 * time.Second))
	if _, err := io.WriteString(c.c, line+"\n"); err != nil {
		c.t.Fatalf("send %q: %v", line, err)
	}
}

func (c *testClient) recv() map[string]any {
	c.t.Helper()
	c.c.SetReadDeadline(time.Now().Add(2 * time.Second))
	line, err := c.br.ReadBytes('\n')
	if err != nil {
		c.t.Fatalf("recv: %v (partial %q)", err, line)
	}
	var m map[string]any
	if err := json.Unmarshal(line, &m); err != nil {
		c.t.Fatalf("recv bad json %q: %v", line, err)
	}
	return m
}

// expectClosed asserts the peer closes without sending anything more.
func (c *testClient) expectClosed() {
	c.t.Helper()
	c.c.SetReadDeadline(time.Now().Add(2 * time.Second))
	data, err := io.ReadAll(c.br)
	if len(data) != 0 {
		c.t.Fatalf("expected no bytes before close, got %q", data)
	}
	var ne net.Error
	if errors.As(err, &ne) && ne.Timeout() {
		c.t.Fatalf("connection was not closed within the deadline")
	}
}

func eventually(t *testing.T, what string, cond func() bool) {
	t.Helper()
	deadline := time.Now().Add(2 * time.Second)
	for time.Now().Before(deadline) {
		if cond() {
			return
		}
		time.Sleep(5 * time.Millisecond)
	}
	t.Fatalf("timed out waiting for: %s", what)
}

// ---- tests ----------------------------------------------------------------

func TestServerRejectsUnknownPeer(t *testing.T) {
	h := newHarness(t, nil)
	h.nextID.Store(777)
	c := h.dial()
	// The server must not read or answer anything for a foreign uid.
	c.send(`{"id":1,"cmd":"hello","protocol":1}`)
	c.expectClosed()
	if got := h.log.snapshot(); len(got) != 0 {
		t.Fatalf("no fake may be touched, got %v", got)
	}
}

func TestServerRejectsPeerUIDError(t *testing.T) {
	h := newHarness(t, func(h *harness) {
		h.srv.PeerUID = func(net.Conn) (uint32, error) { return 0, errors.New("no cred") }
	})
	c := h.dial()
	c.send(`{"id":1,"cmd":"hello","protocol":1}`)
	c.expectClosed()
}

func TestServerAllowsRootAndAllowlisted(t *testing.T) {
	for _, uid := range []uint32{0, testUID} {
		h := newHarness(t, nil)
		h.nextID.Store(uid)
		c := h.dial()
		c.send(`{"id":7,"cmd":"hello","protocol":1}`)
		m := c.recv()
		if m["ok"] != true || m["id"] != float64(7) || m["protocol"] != float64(1) ||
			m["build"] != "1.6.12" || m["singbox"] != "1.13.13" {
			t.Fatalf("uid %d: bad hello reply %v", uid, m)
		}
		if uid != 0 && m["uid"] != float64(uid) {
			t.Fatalf("uid %d: reply uid %v", uid, m["uid"])
		}
	}
}

func TestServerRequiresHelloFirst(t *testing.T) {
	h := newHarness(t, nil)
	c := h.dial()
	c.send(`{"id":1,"cmd":"status"}`)
	c.expectClosed()
}

func TestServerProtocolMismatch(t *testing.T) {
	h := newHarness(t, nil)
	c := h.dial()
	c.send(`{"id":1,"cmd":"hello","protocol":2}`)
	m := c.recv()
	if m["ok"] != false || m["error"] != "protocol mismatch" || m["protocol"] != float64(ProtocolVersion) {
		t.Fatalf("bad mismatch reply %v", m)
	}
	c.expectClosed()
}

func TestServerHandshakeDeadline(t *testing.T) {
	h := newHarness(t, func(h *harness) { h.srv.HandshakeTimeout = 200 * time.Millisecond })
	c := h.dial()
	start := time.Now()
	c.expectClosed()
	if d := time.Since(start); d > 1500*time.Millisecond {
		t.Fatalf("handshake deadline took %v", d)
	}
}

func TestServerHandshakeDeadlineClearedAfterHello(t *testing.T) {
	h := newHarness(t, func(h *harness) { h.srv.HandshakeTimeout = 200 * time.Millisecond })
	c := h.dialAs(testUID)
	time.Sleep(400 * time.Millisecond) // idle longer than the handshake timeout
	c.send(`{"id":2,"cmd":"status"}`)
	if m := c.recv(); m["ok"] != true {
		t.Fatalf("status after idle: %v", m)
	}
}

func TestServerOversizeLine(t *testing.T) {
	h := newHarness(t, nil)
	c := h.dialAs(testUID)
	go func() {
		// No newline: the server must give up at the cap and close.
		_, _ = c.c.Write([]byte(strings.Repeat("a", MaxLineBytes+4096)))
	}()
	c.expectClosed()
}

func TestServerMalformedJSONKeepsConnection(t *testing.T) {
	h := newHarness(t, nil)
	c := h.dialAs(testUID)
	c.send(`{not json`)
	if m := c.recv(); m["ok"] != false || m["error"] == nil {
		t.Fatalf("bad reply %v", m)
	}
	c.send(`{"id":9,"cmd":"status"}`)
	if m := c.recv(); m["ok"] != true || m["id"] != float64(9) {
		t.Fatalf("status after malformed line: %v", m)
	}
}

func TestServerUnknownCommand(t *testing.T) {
	h := newHarness(t, nil)
	c := h.dialAs(testUID)
	c.send(`{"id":3,"cmd":"exec"}`)
	m := c.recv()
	if m["id"] != float64(3) || m["ok"] != false || m["error"] != "unknown command" {
		t.Fatalf("bad reply %v", m)
	}
	c.send(`{"id":4,"cmd":"status"}`)
	if m := c.recv(); m["id"] != float64(4) || m["ok"] != true {
		t.Fatalf("status after unknown: %v", m)
	}
}

func TestServerTunStartValidationError(t *testing.T) {
	h := newHarness(t, nil)
	c := h.dialAs(testUID)

	h.tun.setStartErr(errors.New("config rejected: experimental"))
	c.send(`{"id":2,"cmd":"tun_start","config":"{}","socksPort":2080}`)
	m := c.recv()
	if m["ok"] != false || !strings.Contains(m["error"].(string), "config rejected: experimental") {
		t.Fatalf("bad reply %v", m)
	}
	if n := h.log.count("tun.start"); n != 1 {
		t.Fatalf("Start should have been called once, got %d", n)
	}

	for _, bad := range []string{`0`, `-1`, `65536`, `99999`} {
		c.send(`{"id":3,"cmd":"tun_start","config":"{}","socksPort":` + bad + `}`)
		if m := c.recv(); m["ok"] != false {
			t.Fatalf("port %s accepted: %v", bad, m)
		}
	}
	c.send(`{"id":4,"cmd":"tun_start","socksPort":2080}`)
	if m := c.recv(); m["ok"] != false {
		t.Fatalf("empty config accepted: %v", m)
	}
	if n := h.log.count("tun.start"); n != 1 {
		t.Fatalf("Start must not run for invalid requests, calls=%d", n)
	}
}

func TestServerTunStartOK(t *testing.T) {
	h := newHarness(t, nil)
	c := h.dialAs(testUID)
	c.send(`{"id":2,"cmd":"tun_start","config":"{\"log\":{}}","socksPort":2080}`)
	if m := c.recv(); m["ok"] != true || m["id"] != float64(2) {
		t.Fatalf("bad reply %v", m)
	}
	if cfg, port := h.tun.lastStart(); cfg != `{"log":{}}` || port != 2080 {
		t.Fatalf("Start got %q port %d", cfg, port)
	}
	c.send(`{"id":3,"cmd":"tun_stop"}`)
	if m := c.recv(); m["ok"] != true {
		t.Fatalf("tun_stop: %v", m)
	}
	if n := h.log.count("tun.stop"); n != 1 {
		t.Fatalf("Stop calls %d", n)
	}
}

func TestServerSysproxyApply(t *testing.T) {
	h := newHarness(t, nil)
	c := h.dialAs(testUID)

	bypass, _ := json.Marshal(DefaultBypass())
	c.send(`{"id":2,"cmd":"sysproxy_apply","port":2080,"bypass":` + string(bypass) + `}`)
	m := c.recv()
	if m["ok"] != true || m["id"] != float64(2) {
		t.Fatalf("bad reply %v", m)
	}
	if !reflect.DeepEqual(m["applied"], []any{"Wi-Fi"}) || !reflect.DeepEqual(m["failed"], []any{"Thunderbolt: boom"}) {
		t.Fatalf("applied/failed not relayed: %v", m)
	}
	calls := h.proxy.applyCallsCopy()
	if len(calls) != 1 || calls[0].port != 2080 || !reflect.DeepEqual(calls[0].bypass, DefaultBypass()) {
		t.Fatalf("Apply called with %+v", calls)
	}

	// Invalid input never reaches the backend.
	c.send(`{"id":3,"cmd":"sysproxy_apply","port":2080,"bypass":["ok.com","bad token;rm"]}`)
	if m := c.recv(); m["ok"] != false {
		t.Fatalf("bad bypass accepted: %v", m)
	}
	for _, p := range []string{`0`, `70000`} {
		c.send(`{"id":4,"cmd":"sysproxy_apply","port":` + p + `}`)
		if m := c.recv(); m["ok"] != false {
			t.Fatalf("port %s accepted: %v", p, m)
		}
	}
	if n := len(h.proxy.applyCallsCopy()); n != 1 {
		t.Fatalf("Apply must not run for invalid input, calls=%d", n)
	}
}

func TestServerSysproxyApplyError(t *testing.T) {
	h := newHarness(t, nil)
	h.proxy.setErrors(errors.New("no eligible service"), nil)
	c := h.dialAs(testUID)
	c.send(`{"id":2,"cmd":"sysproxy_apply","port":2080}`)
	m := c.recv()
	if m["ok"] != false || !strings.Contains(m["error"].(string), "no eligible service") {
		t.Fatalf("bad reply %v", m)
	}
}

func TestServerSysproxyRestore(t *testing.T) {
	h := newHarness(t, nil)
	c := h.dialAs(testUID)
	c.send(`{"id":2,"cmd":"sysproxy_apply","port":2080}`)
	c.recv()
	c.send(`{"id":3,"cmd":"sysproxy_restore"}`)
	if m := c.recv(); m["ok"] != true {
		t.Fatalf("restore: %v", m)
	}
	if h.log.count("proxy.restore") != 1 {
		t.Fatalf("Restore not called once: %v", h.log.snapshot())
	}
	h.proxy.setErrors(nil, errors.New("busy"))
	c.send(`{"id":4,"cmd":"sysproxy_restore"}`)
	if m := c.recv(); m["ok"] != false {
		t.Fatalf("restore error not reported: %v", m)
	}
}

func TestServerStatus(t *testing.T) {
	h := newHarness(t, nil)
	c := h.dialAs(testUID)
	c.send(`{"id":2,"cmd":"status"}`)
	m := c.recv()
	if m["ok"] != true || m["tunRunning"] == true || m["proxyApplied"] == true {
		t.Fatalf("idle status %v", m)
	}
	h.tun.mu.Lock()
	h.tun.running = true
	h.tun.mu.Unlock()
	h.proxy.mu.Lock()
	h.proxy.applied = true
	h.proxy.mu.Unlock()
	c.send(`{"id":3,"cmd":"status"}`)
	m = c.recv()
	if m["tunRunning"] != true || m["proxyApplied"] != true {
		t.Fatalf("busy status %v", m)
	}
}

func TestServerUninstall(t *testing.T) {
	release := make(chan struct{})
	h := newHarness(t, func(hh *harness) {
		hh.srv.OnUninstall = func() {
			hh.log.add("uninstall")
			<-release
		}
	})
	c := h.dialAs(testUID)
	c.send(`{"id":2,"cmd":"tun_start","config":"{}","socksPort":2080}`)
	c.recv()
	c.send(`{"id":3,"cmd":"sysproxy_apply","port":2080}`)
	c.recv()

	c.send(`{"id":4,"cmd":"uninstall"}`)
	// OnUninstall blocks until released, so receiving the reply proves it was
	// sent first.
	m := c.recv()
	if m["ok"] != true || m["id"] != float64(4) {
		t.Fatalf("bad uninstall reply %v", m)
	}
	eventually(t, "OnUninstall", func() bool { return h.log.count("uninstall") == 1 })
	close(release)

	calls := h.log.snapshot()
	idx := func(name string) int {
		for i, c := range calls {
			if c == name {
				return i
			}
		}
		return -1
	}
	if idx("tun.stop") < 0 || idx("proxy.restore") < 0 ||
		idx("uninstall") < idx("tun.stop") || idx("uninstall") < idx("proxy.restore") {
		t.Fatalf("uninstall must run after stop and restore: %v", calls)
	}
	if h.log.count("uninstall") != 1 {
		t.Fatalf("OnUninstall calls: %v", calls)
	}
}

func TestServerUninstallAbortsWhenRestoreFails(t *testing.T) {
	h := newHarness(t, func(hh *harness) {
		hh.srv.OnUninstall = func() { hh.log.add("uninstall") }
	})
	h.proxy.setErrors(nil, errors.New("networksetup failed"))
	c := h.dialAs(testUID)
	c.send(`{"id":2,"cmd":"uninstall"}`)
	if m := c.recv(); m["ok"] != false {
		t.Fatalf("uninstall must fail when the proxy cannot be restored: %v", m)
	}
	time.Sleep(50 * time.Millisecond)
	if h.log.count("uninstall") != 0 {
		t.Fatalf("OnUninstall ran despite restore failure")
	}
}

func TestServerLogsCommandsWithoutConfigBody(t *testing.T) {
	h := newHarness(t, nil)
	c := h.dialAs(testUID)
	c.send(`{"id":2,"cmd":"tun_start","config":"SECRET-CONFIG-BODY","socksPort":2080}`)
	c.recv()
	got := h.logged()
	if !strings.Contains(got, "tun_start") || !strings.Contains(got, "501") {
		t.Fatalf("command/uid not logged: %q", got)
	}
	if strings.Contains(got, "SECRET-CONFIG-BODY") {
		t.Fatalf("config body leaked into the log: %q", got)
	}
}

func TestServerServeRequiresPeerUID(t *testing.T) {
	s := &Server{Tun: &fakeTun{log: &callLog{}}, Proxy: &fakeProxy{log: &callLog{}}}
	dir, err := os.MkdirTemp("", "mh")
	if err != nil {
		t.Fatal(err)
	}
	defer os.RemoveAll(dir)
	l, err := net.Listen("unix", filepath.Join(dir, "s"))
	if err != nil {
		t.Fatal(err)
	}
	defer l.Close()
	if err := s.Serve(l); err == nil {
		t.Fatal("Serve must refuse to run without a PeerUID check")
	}
}

var _ ProxyBackend = (*fakeProxy)(nil)
var _ TunRunner = (*fakeTun)(nil)

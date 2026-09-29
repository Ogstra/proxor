package machelper

import (
	"errors"
	"net"
	"strconv"
	"strings"
	"testing"
	"time"
)

const settle = 150 * time.Millisecond // time given to an async cleanup that must NOT happen

// expectSilence asserts nothing arrives on c for a short while.
func (c *testClient) expectSilence() {
	c.t.Helper()
	c.c.SetReadDeadline(time.Now().Add(settle))
	line, err := c.br.ReadBytes('\n')
	if err == nil {
		c.t.Fatalf("unexpected message: %s", line)
	}
	var ne net.Error
	if !errors.As(err, &ne) || !ne.Timeout() {
		c.t.Fatalf("connection closed unexpectedly: %v", err)
	}
}

func (c *testClient) startTun(id int) {
	c.t.Helper()
	c.send(`{"id":` + strconv.Itoa(id) + `,"cmd":"tun_start","config":"{}","socksPort":2080}`)
	if m := c.recv(); m["ok"] != true {
		c.t.Fatalf("tun_start: %v", m)
	}
}

func (c *testClient) applyProxy(id int) {
	c.t.Helper()
	c.send(`{"id":` + strconv.Itoa(id) + `,"cmd":"sysproxy_apply","port":2080}`)
	if m := c.recv(); m["ok"] != true {
		c.t.Fatalf("sysproxy_apply: %v", m)
	}
}

func TestLeaseCloseStopsTun(t *testing.T) {
	h := newHarness(t, nil)
	a := h.dialAs(testUID)
	a.startTun(2)
	a.c.Close()
	eventually(t, "Tun.Stop after owner close", func() bool { return h.log.count("tun.stop") == 1 })
	if h.tun.Running() {
		t.Fatal("Tun still running after owner closed")
	}
}

func TestLeaseCloseRestoresProxy(t *testing.T) {
	h := newHarness(t, nil)
	a := h.dialAs(testUID)
	a.applyProxy(2)
	a.c.Close()
	eventually(t, "Proxy.Restore after owner close", func() bool { return h.log.count("proxy.restore") == 1 })
	if h.proxy.Applied() {
		t.Fatal("proxy still applied after owner closed")
	}
}

func TestLeaseOtherConnectionDoesNotCleanup(t *testing.T) {
	h := newHarness(t, nil)
	a := h.dialAs(testUID)
	a.startTun(2)
	a.applyProxy(3)
	b := h.dialAs(testUID)
	b.c.Close()
	time.Sleep(settle)
	if h.log.count("proxy.restore") != 0 || h.log.count("tun.stop") != 0 {
		t.Fatalf("a non-owner closing cleaned up: %v", h.log.snapshot())
	}
	a.expectSilence()
}

func TestLeaseTakeover(t *testing.T) {
	h := newHarness(t, nil)
	a := h.dialAs(testUID)
	a.startTun(2)
	b := h.dialAs(testUID)
	b.startTun(2)

	want := []string{"tun.start", "tun.stop", "tun.start"}
	got := h.log.snapshot()
	if strings.Join(got, ",") != strings.Join(want, ",") {
		t.Fatalf("takeover order: got %v want %v", got, want)
	}
	// The previous owner is told it lost the Tun.
	m := a.recv()
	if m["event"] != EventTunStopped || !strings.Contains(m["reason"].(string), "another connection") {
		t.Fatalf("previous owner got %v", m)
	}
	// A closing later must not stop B's Tun.
	a.c.Close()
	time.Sleep(settle)
	if n := h.log.count("tun.stop"); n != 1 {
		t.Fatalf("old owner close stopped the new Tun (stop calls %d)", n)
	}
	// B closing does.
	b.c.Close()
	eventually(t, "Stop after new owner closes", func() bool { return h.log.count("tun.stop") == 2 })
}

func TestLeaseRestartOnSameConnectionIsSilent(t *testing.T) {
	h := newHarness(t, nil)
	a := h.dialAs(testUID)
	a.startTun(2)
	a.startTun(3) // GUI restart: stop, start again, ownership stays
	a.expectSilence()
	a.c.Close()
	eventually(t, "Stop after owner close", func() bool { return h.log.count("tun.stop") == 2 })
}

func TestLeaseEventsToOwnerOnly(t *testing.T) {
	h := newHarness(t, nil)
	a := h.dialAs(testUID)
	b := h.dialAs(testUID)
	a.startTun(2)

	h.tun.emitter(0)(Event{Event: EventTunReady})
	if m := a.recv(); m["event"] != EventTunReady {
		t.Fatalf("owner got %v", m)
	}
	h.tun.emitter(0)(Event{Event: EventLog, Line: "hello"})
	if m := a.recv(); m["event"] != EventLog || m["line"] != "hello" {
		t.Fatalf("owner log got %v", m)
	}
	b.expectSilence()

	// Takeover: B owns the new instance; the stale instance is muted.
	b.startTun(2)
	if m := a.recv(); m["event"] != EventTunStopped { // told about the takeover
		t.Fatalf("expected takeover notice, got %v", m)
	}
	h.tun.emitter(0)(Event{Event: EventLog, Line: "stale"})
	a.expectSilence()
	b.expectSilence()

	h.tun.emitter(1)(Event{Event: EventTunReady})
	if m := b.recv(); m["event"] != EventTunReady {
		t.Fatalf("new owner got %v", m)
	}
	a.expectSilence()
}

func TestLeaseTunStoppedClearsOwnership(t *testing.T) {
	h := newHarness(t, nil)
	a := h.dialAs(testUID)
	a.startTun(2)
	h.tun.emitter(0)(Event{Event: EventTunStopped, Reason: "sing-box exited"})
	if m := a.recv(); m["event"] != EventTunStopped || m["reason"] != "sing-box exited" {
		t.Fatalf("owner got %v", m)
	}
	a.c.Close()
	time.Sleep(settle)
	if n := h.log.count("tun.stop"); n != 0 {
		t.Fatalf("Stop called for an instance that already ended (%d)", n)
	}
}

func TestLeaseStartFailureLeavesNoOwner(t *testing.T) {
	h := newHarness(t, nil)
	a := h.dialAs(testUID)
	h.tun.setStartErr(errors.New("rejected"))
	a.send(`{"id":2,"cmd":"tun_start","config":"{}","socksPort":2080}`)
	if m := a.recv(); m["ok"] != false {
		t.Fatalf("expected failure, got %v", m)
	}
	a.c.Close()
	time.Sleep(settle)
	if n := h.log.count("tun.stop"); n != 0 {
		t.Fatalf("Stop called although nothing started (%d)", n)
	}
}

func TestLeaseTunStopByAnyConnectionClearsOwner(t *testing.T) {
	h := newHarness(t, nil)
	a := h.dialAs(testUID)
	a.startTun(2)
	b := h.dialAs(testUID)
	b.send(`{"id":3,"cmd":"tun_stop"}`)
	if m := b.recv(); m["ok"] != true {
		t.Fatalf("tun_stop: %v", m)
	}
	a.c.Close()
	time.Sleep(settle)
	if n := h.log.count("tun.stop"); n != 1 {
		t.Fatalf("Stop calls %d, want 1", n)
	}
}

func TestLeaseProxyRestoreClearsOwner(t *testing.T) {
	h := newHarness(t, nil)
	a := h.dialAs(testUID)
	a.applyProxy(2)
	// A non-owner may restore (GUI after "restart program" has a new lease).
	b := h.dialAs(testUID)
	b.send(`{"id":3,"cmd":"sysproxy_restore"}`)
	if m := b.recv(); m["ok"] != true {
		t.Fatalf("restore: %v", m)
	}
	a.c.Close()
	time.Sleep(settle)
	if n := h.log.count("proxy.restore"); n != 1 {
		t.Fatalf("Restore calls %d, want 1", n)
	}
}

func TestLeaseFailedRestoreIsRetriedOnClose(t *testing.T) {
	h := newHarness(t, nil)
	a := h.dialAs(testUID)
	a.applyProxy(2)
	h.proxy.setErrors(nil, errors.New("busy"))
	a.send(`{"id":3,"cmd":"sysproxy_restore"}`)
	if m := a.recv(); m["ok"] != false {
		t.Fatalf("restore should fail: %v", m)
	}
	h.proxy.setErrors(nil, nil)
	a.c.Close()
	eventually(t, "second Restore on close", func() bool { return h.log.count("proxy.restore") == 2 })
}

func TestLeaseProxyOwnershipMoves(t *testing.T) {
	h := newHarness(t, nil)
	a := h.dialAs(testUID)
	a.applyProxy(2)
	b := h.dialAs(testUID)
	b.applyProxy(2)
	a.c.Close()
	time.Sleep(settle)
	if n := h.log.count("proxy.restore"); n != 0 {
		t.Fatalf("old owner close restored the proxy (%d)", n)
	}
	b.c.Close()
	eventually(t, "Restore after new owner closes", func() bool { return h.log.count("proxy.restore") == 1 })
}

func TestShutdownCleansEverything(t *testing.T) {
	h := newHarness(t, nil)
	a := h.dialAs(testUID)
	a.startTun(2)
	a.applyProxy(3)

	h.srv.Shutdown()
	if h.log.count("tun.stop") != 1 || h.log.count("proxy.restore") != 1 {
		t.Fatalf("Shutdown: %v", h.log.snapshot())
	}
	// The clients are dropped and their leases are already gone.
	a.expectClosed()
	time.Sleep(settle)
	if h.log.count("tun.stop") != 1 || h.log.count("proxy.restore") != 1 {
		t.Fatalf("cleanup ran twice: %v", h.log.snapshot())
	}
	// Nothing new may start once the helper is shutting down.
	b := h.dial()
	// The server may already have hung up, so the write can fail with EPIPE.
	_, _ = b.c.Write([]byte(`{"id":1,"cmd":"hello","protocol":1}` + "\n"))
	b.expectClosed()
	if n := h.log.count("tun.start"); n != 1 {
		t.Fatalf("tun.start calls %d", n)
	}
}

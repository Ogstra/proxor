package speedtest

import (
	"context"
	"errors"
	"net"
	"os"
	"runtime"
	"strconv"
	"strings"
	"syscall"
	"testing"
)

// icmpExpectedOnThisRunner reports whether an unprivileged/privileged ICMP
// socket can be opened on this machine.
func icmpExpectedOnThisRunner(t *testing.T) bool {
	t.Helper()
	switch runtime.GOOS {
	case "windows", "darwin":
		t.Logf("icmp path: %s (always available)", runtime.GOOS)
		return true
	case "linux":
		if os.Geteuid() == 0 {
			t.Logf("icmp path: linux root")
			return true
		}
		b, err := os.ReadFile("/proc/sys/net/ipv4/ping_group_range")
		if err != nil {
			t.Logf("icmp path: linux, cannot read ping_group_range: %v", err)
			return false
		}
		f := strings.Fields(string(b))
		if len(f) != 2 {
			return false
		}
		lo, e1 := strconv.Atoi(f[0])
		hi, e2 := strconv.Atoi(f[1])
		if e1 != nil || e2 != nil {
			return false
		}
		gid := os.Getegid()
		ok := gid >= lo && gid <= hi
		t.Logf("icmp path: linux ping_group_range=%d-%d egid=%d allowed=%v", lo, hi, gid, ok)
		return ok
	}
	return false
}

func TestLocalIcmpErrorContract(t *testing.T) {
	if LocalIcmpErrorPrefix != "icmp-unavailable: " {
		t.Fatalf("prefix literal changed: %q", LocalIcmpErrorPrefix)
	}
	e := &LocalIcmpError{Cause: syscall.EPERM}
	if !strings.HasPrefix(e.Error(), "icmp-unavailable: ") {
		t.Fatalf("missing prefix: %q", e.Error())
	}
	if !errors.Is(e, ErrIcmpUnavailable) {
		t.Fatal("errors.Is(ErrIcmpUnavailable) false")
	}
	if !errors.Is(e, syscall.EPERM) {
		t.Fatal("Unwrap does not reach cause")
	}
}

func TestIsLocalSocketError(t *testing.T) {
	wrapSys := func(e error) error { return os.NewSyscallError("socket", e) }
	wrapOp := func(e error) error { return &net.OpError{Op: "listen", Err: os.NewSyscallError("socket", e)} }
	cases := []struct {
		name string
		err  error
		want bool
	}{
		{"EPERM", wrapSys(syscall.EPERM), true},
		{"EACCES", wrapSys(syscall.EACCES), true},
		{"EPROTONOSUPPORT", wrapSys(syscall.EPROTONOSUPPORT), true},
		{"EAFNOSUPPORT", wrapSys(syscall.EAFNOSUPPORT), true},
		{"OpError EPERM", wrapOp(syscall.EPERM), true},
		{"OpError EACCES", wrapOp(syscall.EACCES), true},
		{"ECONNREFUSED", wrapSys(syscall.ECONNREFUSED), false},
		{"ETIMEDOUT", wrapSys(syscall.ETIMEDOUT), false},
		{"deadline", context.DeadlineExceeded, false},
		{"io timeout", errors.New("i/o timeout"), false},
		{"nil", nil, false},
	}
	for _, c := range cases {
		if got := IsLocalSocketError(c.err); got != c.want {
			t.Errorf("%s: got %v want %v", c.name, got, c.want)
		}
	}
}

func TestIcmpPingIPv6OnlyTargetIsLocal(t *testing.T) {
	for _, a := range []string{"[::1]:443", "::1"} {
		_, err := IcmpPing(a, 500)
		if err == nil || !errors.Is(err, ErrIcmpUnavailable) {
			t.Errorf("%s: expected ErrIcmpUnavailable, got %v", a, err)
		}
	}
}

func checkLoopback(t *testing.T, addr string) {
	t.Helper()
	ms, err := IcmpPing(addr, 2000)
	if icmpExpectedOnThisRunner(t) {
		if err != nil {
			t.Fatalf("%s: expected success, got %v", addr, err)
		}
		if ms < 0 {
			t.Fatalf("negative rtt %d", ms)
		}
		return
	}
	if err == nil || !errors.Is(err, ErrIcmpUnavailable) {
		t.Fatalf("%s: expected ErrIcmpUnavailable, got ms=%d err=%v", addr, ms, err)
	}
}

func TestIcmpPingLoopback(t *testing.T) { checkLoopback(t, "127.0.0.1") }

func TestIcmpPingHostPortForm(t *testing.T) { checkLoopback(t, "127.0.0.1:443") }

func TestIcmpPingNoReplyIsNotLocal(t *testing.T) {
	if !icmpExpectedOnThisRunner(t) {
		t.Skip("ICMP not permitted on this runner")
	}
	_, err := IcmpPing("192.0.2.1", 300)
	if err == nil {
		t.Skip("TEST-NET-1 unexpectedly answered")
	}
	if errors.Is(err, ErrIcmpUnavailable) {
		// A sandbox without any route can surface EPERM/ENETUNREACH on write;
		// only EPERM-style local errors are tolerated by the contract.
		t.Fatalf("no-reply must not be classified local: %v", err)
	}
}

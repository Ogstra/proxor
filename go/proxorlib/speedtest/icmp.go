package speedtest

import (
	"errors"
	"fmt"
	"net"
	"sync/atomic"
	"syscall"
)

// LocalIcmpErrorPrefix starts every error that means "this machine cannot send
// ICMP" (as opposed to "the server did not answer"). The GUI matches this exact
// string (src/platform/PingPolicy.hpp) to fall back to TCP ping;
// test/package_mode/wiring.d/ping.sh enforces that both literals are equal.
const LocalIcmpErrorPrefix = "icmp-unavailable: "

// ErrIcmpUnavailable is matched with errors.Is against a *LocalIcmpError.
var ErrIcmpUnavailable = errors.New("icmp unavailable on this system")

// LocalIcmpError is a local inability to ping (permission, unsupported family).
type LocalIcmpError struct{ Cause error }

func (e *LocalIcmpError) Error() string {
	if e.Cause == nil {
		return LocalIcmpErrorPrefix + ErrIcmpUnavailable.Error()
	}
	return LocalIcmpErrorPrefix + e.Cause.Error()
}

func (e *LocalIcmpError) Unwrap() error { return e.Cause }

func (e *LocalIcmpError) Is(target error) bool { return target == ErrIcmpUnavailable }

// IsLocalSocketError reports whether err is a local permission/support failure.
func IsLocalSocketError(err error) bool {
	if err == nil {
		return false
	}
	for _, e := range []error{syscall.EPERM, syscall.EACCES, syscall.EPROTONOSUPPORT, syscall.EAFNOSUPPORT} {
		if errors.Is(err, e) {
			return true
		}
	}
	for _, e := range platformLocalErrnos {
		if errors.Is(err, e) {
			return true
		}
	}
	return false
}

var icmpSeq atomic.Uint32

// IcmpPing pings address (bare host or host:port) over IPv4 ICMP.
func IcmpPing(address string, timeout int32) (int32, error) {
	host := address
	if h, _, err := net.SplitHostPort(address); err == nil {
		host = h
	}
	dst, err := net.ResolveIPAddr("ip4", host)
	if err != nil {
		if _, err6 := net.ResolveIPAddr("ip6", host); err6 == nil {
			return 0, &LocalIcmpError{Cause: fmt.Errorf("ICMP ping supports IPv4 targets only, %s is IPv6", host)}
		}
		return 0, err
	}
	return pingIPv4(dst.IP.To4(), timeout)
}

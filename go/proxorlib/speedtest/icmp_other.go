//go:build !darwin && !linux && !windows

package speedtest

import (
	"errors"
	"net"
	"runtime"
)

var platformLocalErrnos []error

func pingIPv4(dst net.IP, timeout int32) (int32, error) {
	return 0, &LocalIcmpError{Cause: errors.New("ICMP ping is not implemented on " + runtime.GOOS)}
}

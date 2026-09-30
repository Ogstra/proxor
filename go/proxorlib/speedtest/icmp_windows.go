//go:build windows

package speedtest

import (
	"encoding/binary"
	"errors"
	"fmt"
	"net"
	"syscall"
	"unsafe"
)

var platformLocalErrnos = []error{syscall.Errno(10013), syscall.Errno(5)}

var (
	iphlpapiDLL         = syscall.NewLazyDLL("iphlpapi.dll")
	procIcmpCreateFile  = iphlpapiDLL.NewProc("IcmpCreateFile")
	procIcmpSendEcho    = iphlpapiDLL.NewProc("IcmpSendEcho")
	procIcmpCloseHandle = iphlpapiDLL.NewProc("IcmpCloseHandle")
)

const (
	errIPReqTimedOut   = syscall.Errno(11010)
	errNotSupported    = syscall.Errno(50)
	invalidHandleValue = ^uintptr(0)
)

func pingIPv4(dst net.IP, timeout int32) (int32, error) {
	if err := procIcmpSendEcho.Find(); err != nil {
		return 0, &LocalIcmpError{Cause: err}
	}
	h, _, callErr := procIcmpCreateFile.Call()
	if h == invalidHandleValue || h == 0 {
		return 0, &LocalIcmpError{Cause: fmt.Errorf("IcmpCreateFile: %v", callErr)}
	}
	defer procIcmpCloseHandle.Call(h)

	data := []byte("proxor")
	reply := make([]byte, 256)
	dest := binary.LittleEndian.Uint32(dst)
	n, _, callErr := procIcmpSendEcho.Call(
		h,
		uintptr(dest),
		uintptr(unsafe.Pointer(&data[0])),
		uintptr(len(data)),
		0,
		uintptr(unsafe.Pointer(&reply[0])),
		uintptr(len(reply)),
		uintptr(uint32(timeout)),
	)
	if n == 0 {
		var errno syscall.Errno
		if !errors.As(callErr, &errno) {
			return 0, callErr
		}
		switch {
		case errno == errIPReqTimedOut:
			return 0, errors.New("icmp: request timed out")
		case errno == errNotSupported || IsLocalSocketError(errno):
			return 0, &LocalIcmpError{Cause: errno}
		}
		return 0, errno
	}
	status := binary.LittleEndian.Uint32(reply[4:8])
	rtt := binary.LittleEndian.Uint32(reply[8:12])
	if status != 0 {
		return 0, fmt.Errorf("icmp: reply status %d", status)
	}
	return int32(rtt), nil
}

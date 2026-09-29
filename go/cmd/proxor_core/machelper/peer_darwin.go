//go:build darwin

package machelper

import (
	"errors"
	"fmt"
	"net"

	"golang.org/x/sys/unix"
)

// PeerUID returns the effective uid of the process on the other end of a unix
// socket, as recorded by the kernel when the connection was made
// (the peer-credential socket option). It cannot be spoofed by the client, and nothing the client
// sends or any environment variable is consulted.
func PeerUID(c net.Conn) (uint32, error) {
	uc, ok := c.(*net.UnixConn)
	if !ok {
		return 0, fmt.Errorf("peer credentials need a unix connection, got %T", c)
	}
	raw, err := uc.SyscallConn()
	if err != nil {
		return 0, err
	}
	var cred *unix.Xucred
	var sockErr error
	if err := raw.Control(func(fd uintptr) {
		cred, sockErr = unix.GetsockoptXucred(int(fd), unix.SOL_LOCAL, unix.LOCAL_PEERCRED)
	}); err != nil {
		return 0, err
	}
	if sockErr != nil {
		return 0, sockErr
	}
	if cred == nil {
		return 0, errors.New("no peer credentials")
	}
	return cred.Uid, nil
}

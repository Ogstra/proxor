//go:build !darwin

package machelper

import (
	"errors"
	"net"
)

// PeerUID fails closed: the helper only runs on macOS, so on any other platform
// no peer is ever trusted.
func PeerUID(c net.Conn) (uint32, error) {
	return 0, errors.New("peer credentials are only implemented on macOS")
}

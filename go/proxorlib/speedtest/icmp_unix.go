//go:build darwin || linux

package speedtest

import (
	"fmt"
	"net"
	"os"
	"time"

	"golang.org/x/net/icmp"
	"golang.org/x/net/ipv4"
)

var platformLocalErrnos []error

func pingIPv4(dst net.IP, timeout int32) (int32, error) {
	var conn *icmp.PacketConn
	var raw bool
	var errs [2]error
	for i, network := range []string{"udp4", "ip4:icmp"} {
		c, err := icmp.ListenPacket(network, "0.0.0.0")
		if err == nil {
			conn, raw = c, network != "udp4"
			break
		}
		if !IsLocalSocketError(err) {
			return 0, err
		}
		errs[i] = err
	}
	if conn == nil {
		return 0, &LocalIcmpError{Cause: fmt.Errorf("udp4: %v; raw: %v", errs[0], errs[1])}
	}
	defer conn.Close()

	seq := int(icmpSeq.Add(1) & 0xffff)
	id := os.Getpid() & 0xffff
	msg := icmp.Message{
		Type: ipv4.ICMPTypeEcho,
		Code: 0,
		Body: &icmp.Echo{ID: id, Seq: seq, Data: []byte("proxor")},
	}
	wb, err := msg.Marshal(nil)
	if err != nil {
		return 0, err
	}
	var to net.Addr = &net.UDPAddr{IP: dst}
	if raw {
		to = &net.IPAddr{IP: dst}
	}

	start := time.Now()
	if _, err := conn.WriteTo(wb, to); err != nil {
		if IsLocalSocketError(err) {
			return 0, &LocalIcmpError{Cause: err}
		}
		return 0, err
	}
	if err := conn.SetReadDeadline(start.Add(time.Duration(timeout) * time.Millisecond)); err != nil {
		return 0, err
	}

	rb := make([]byte, 1500)
	for {
		n, peer, err := conn.ReadFrom(rb)
		if err != nil {
			return 0, err
		}
		rm, err := icmp.ParseMessage(1, rb[:n])
		if err != nil || rm.Type != ipv4.ICMPTypeEchoReply {
			continue
		}
		var peerIP net.IP
		switch p := peer.(type) {
		case *net.UDPAddr:
			peerIP = p.IP
		case *net.IPAddr:
			peerIP = p.IP
		}
		if !peerIP.Equal(dst) {
			continue
		}
		echo, ok := rm.Body.(*icmp.Echo)
		if !ok || echo.Seq != seq {
			continue
		}
		if raw && echo.ID != id {
			continue
		}
		return int32(time.Since(start).Milliseconds()), nil
	}
}

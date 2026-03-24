package main

import (
	"context"
	"net"
	"net/http"

	"github.com/Ogstra/proxorlib/proxor_common"
	"github.com/Ogstra/proxorlib/proxor_log"
	box "github.com/sagernet/sing-box"
	"github.com/sagernet/sing-box/boxapi"
	boxmain "proxor_core/boxmain"
)

var instance *box.Box
var instance_cancel context.CancelFunc

func setupCore() {
	boxmain.SetDisableColor(true)
	proxor_log.SetupLog(50*1024, "./proxor.log")
	// Do NOT set a platform log writer — sing-box logs via its regular logger
	// (stderr, DisableColor=true → plain text). Using a platform writer caused
	// duplicate lines: once via the platform writer (stdout) and once via the
	// regular logger (stderr). Thunder uses the same approach.
	proxor_common.GetCurrentInstance = func() interface{} {
		return instance
	}
	proxor_common.DialContext = func(ctx context.Context, specifiedInstance interface{}, network, addr string) (net.Conn, error) {
		if i, ok := specifiedInstance.(*box.Box); ok {
			return boxapi.DialContext(ctx, i, network, addr)
		}
		if instance != nil {
			return boxapi.DialContext(ctx, instance, network, addr)
		}
		return proxor_common.DialContextSystem(ctx, network, addr)
	}
	proxor_common.DialUDP = func(ctx context.Context, specifiedInstance interface{}) (net.PacketConn, error) {
		if i, ok := specifiedInstance.(*box.Box); ok {
			return boxapi.DialUDP(ctx, i)
		}
		if instance != nil {
			return boxapi.DialUDP(ctx, instance)
		}
		return proxor_common.DialUDPSystem(ctx)
	}
	proxor_common.CreateProxyHttpClient = func(specifiedInstance interface{}) *http.Client {
		if i, ok := specifiedInstance.(*box.Box); ok {
			return boxapi.CreateProxyHttpClient(i)
		}
		return boxapi.CreateProxyHttpClient(instance)
	}
}

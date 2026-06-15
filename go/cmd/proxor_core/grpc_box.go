package main

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"net"
	"os"
	"os/exec"
	"runtime"
	"strings"
	"time"

	"grpc_server"
	"grpc_server/gen"

	"github.com/Ogstra/proxorlib/proxor_common"
	"github.com/Ogstra/proxorlib/speedtest"
	"github.com/sagernet/sing-box/adapter"
	box "github.com/sagernet/sing-box"
	"github.com/sagernet/sing-box/boxapi"
	"github.com/sagernet/sing-box/experimental/clashapi"
	boxmain "proxor_core/boxmain"

	"log"

	"github.com/sagernet/sing-box/option"
	"github.com/sagernet/sing/service"
)

type server struct {
	grpc_server.BaseServer
}

func isTunAddressConflict(err error) bool {
	if err == nil {
		return false
	}
	msg := strings.ToLower(err.Error())
	return strings.Contains(msg, "already exists") && strings.Contains(msg, "address")
}

func tunAddressesFromConfig(coreConfig string) []string {
	var cfg struct {
		Inbounds []struct {
			Type    string   `json:"type"`
			Address []string `json:"address"`
		} `json:"inbounds"`
	}
	if err := json.Unmarshal([]byte(coreConfig), &cfg); err != nil {
		return nil
	}
	var out []string
	for _, ib := range cfg.Inbounds {
		if ib.Type != "tun" {
			continue
		}
		for _, a := range ib.Address {
			ip, _, err := net.ParseCIDR(a)
			if err != nil {
				ip = net.ParseIP(a)
			}
			if ip != nil && ip.To4() != nil {
				out = append(out, ip.String())
			}
		}
	}
	return out
}

func cleanupLeftoverTunAddress(coreConfig string) {
	if runtime.GOOS != "windows" {
		return
	}
	addrs := tunAddressesFromConfig(coreConfig)
	if len(addrs) == 0 {
		return
	}
	ifaces, err := net.Interfaces()
	if err != nil {
		return
	}
	for _, ifi := range ifaces {
		ifAddrs, err := ifi.Addrs()
		if err != nil {
			continue
		}
		for _, a := range ifAddrs {
			ip, _, err := net.ParseCIDR(a.String())
			if err != nil {
				continue
			}
			for _, target := range addrs {
				if ip.String() == target {
					_ = exec.Command("netsh", "interface", "ipv4", "delete", "address", "name="+ifi.Name, "address="+target).Run()
				}
			}
		}
	}
}

func (s *server) Validate(ctx context.Context, in *gen.LoadConfigReq) (out *gen.ErrorResp, _ error) {
	var err error

	defer func() {
		out = &gen.ErrorResp{}
		if err != nil {
			out.Error = err.Error()
		}
	}()

	err = boxmain.Check([]byte(in.CoreConfig))
	return
}

func (s *server) Start(ctx context.Context, in *gen.LoadConfigReq) (out *gen.ErrorResp, _ error) {
	var err error

	defer func() {
		out = &gen.ErrorResp{}
		if err != nil {
			out.Error = err.Error()
			instance = nil
		}
	}()

	if proxor_common.Debug {
		log.Println("Start:", in.CoreConfig)
	}

	if instance != nil {
		err = errors.New("instance already started")
		return
	}

	instance, instance_cancel, err = boxmain.Create([]byte(in.CoreConfig))

	// A previous unclean exit can leave the wintun adapter holding the TUN address,
	// making startup fail with "set ipv4 address: the object already exists".
	// Remove the leftover address and retry.
	for attempt := 0; attempt < 3 && err != nil && isTunAddressConflict(err); attempt++ {
		if instance != nil {
			instance.Close()
			instance = nil
		}
		cleanupLeftoverTunAddress(in.CoreConfig)
		time.Sleep(300 * time.Millisecond)
		instance, instance_cancel, err = boxmain.Create([]byte(in.CoreConfig))
	}

	if instance != nil {
		// V2ray Service
		if in.StatsOutbounds != nil {
			v2rayServer := boxapi.NewSbV2rayServer(option.V2RayStatsServiceOptions{
				Enabled:   true,
				Outbounds: in.StatsOutbounds,
			})
			instance.Router().SetV2RayServer(v2rayServer)
			if v2rayServer.StatsService() != nil {
				instance.Router().AppendTracker(v2rayServer.StatsService())
			}
		}
	}

	return
}

func (s *server) Stop(ctx context.Context, in *gen.EmptyReq) (out *gen.ErrorResp, _ error) {
	var err error

	defer func() {
		out = &gen.ErrorResp{}
		if err != nil {
			out.Error = err.Error()
		}
	}()

	if instance == nil {
		return
	}

	instance_cancel()
	instance.Close()

	instance = nil

	return
}

func (s *server) Test(ctx context.Context, in *gen.TestReq) (out *gen.TestResp, _ error) {
	var err error
	out = &gen.TestResp{Ms: 0}

	defer func() {
		if err != nil {
			out.Error = err.Error()
		}
	}()

	if in.Mode == gen.TestMode_UrlTest || in.Mode == gen.TestMode_HeadPing {
		var i *box.Box
		var cancel context.CancelFunc
		if in.Config != nil {
			// Test instance
			i, cancel, err = boxmain.Create([]byte(in.Config.CoreConfig))
			if i != nil {
				defer i.Close()
				defer cancel()
			}
			if err != nil {
				return
			}
		} else {
			// Test running instance
			i = instance
			if i == nil {
				return
			}
		}
		method := "GET"
		if in.Mode == gen.TestMode_HeadPing {
			method = "HEAD"
		}
		out.Ms, err = speedtest.UrlTest(boxapi.CreateProxyHttpClient(i), in.Url, in.Timeout, speedtest.UrlTestStandard_RTT, method)
	} else if in.Mode == gen.TestMode_TcpPing {
		out.Ms, err = speedtest.TcpPing(in.Address, in.Timeout)
	} else if in.Mode == gen.TestMode_IcmpPing {
		out.Ms, err = speedtest.IcmpPing(in.Address, in.Timeout)
	} else if in.Mode == gen.TestMode_FullTest {
		i, cancel, err := boxmain.Create([]byte(in.Config.CoreConfig))
		if i != nil {
			defer i.Close()
			defer cancel()
		}
		if err != nil {
			return
		}
		return grpc_server.DoFullTest(ctx, in, i)
	}

	return
}

func (s *server) QueryStats(ctx context.Context, in *gen.QueryStatsReq) (out *gen.QueryStatsResp, _ error) {
	out = &gen.QueryStatsResp{}

	if instance != nil {
		if ss, ok := instance.Router().V2RayServer().(*boxapi.SbV2rayServer); ok {
			out.Traffic = ss.QueryStats(fmt.Sprintf("outbound>>>%s>>>traffic>>>%s", in.Tag, in.Direct))
		}
	}

	return
}

func (s *server) ListConnections(ctx context.Context, in *gen.EmptyReq) (*gen.ListConnectionsResp, error) {
	out := &gen.ListConnectionsResp{}
	if instance == nil {
		return out, nil
	}
	clashServer := service.FromContext[adapter.ClashServer](boxmain.CurrentInstanceContext())
	if clashServer == nil {
		return out, nil
	}
	clash, ok := clashServer.(*clashapi.Server)
	if !ok {
		return out, nil
	}

	connections := clash.TrafficManager().Connections()
	items := make([]map[string]any, 0, len(connections))
	for index, c := range connections {
		dest := c.Metadata.Destination.String()
		resolvedDest := c.Metadata.Domain
		if resolvedDest == "" || resolvedDest == c.Metadata.Destination.Fqdn {
			resolvedDest = ""
		}
		process := ""
		if c.Metadata.ProcessInfo != nil {
			if c.Metadata.ProcessInfo.ProcessPath != "" {
				parts := strings.Split(c.Metadata.ProcessInfo.ProcessPath, string(os.PathSeparator))
				process = parts[len(parts)-1]
			} else if len(c.Metadata.ProcessInfo.AndroidPackageNames) > 0 {
				process = c.Metadata.ProcessInfo.AndroidPackageNames[0]
			} else if c.Metadata.ProcessInfo.UserName != "" {
				process = c.Metadata.ProcessInfo.UserName
			} else if c.Metadata.ProcessInfo.UserId >= 0 {
				process = fmt.Sprintf("uid:%d", c.Metadata.ProcessInfo.UserId)
			} else if c.Metadata.ProcessInfo.ProcessID > 0 {
				process = fmt.Sprintf("pid:%d", c.Metadata.ProcessInfo.ProcessID)
			}
		}
		items = append(items, map[string]any{
			"ID":      index + 1,
			"Tag":     c.Outbound,
			"Start":   c.CreatedAt.Unix(),
			"End":     0,
			"Dest":    dest,
			"RDest":   resolvedDest,
			"Process": process,
			"Upload":  c.Upload.Load(),
			"Download": c.Download.Load(),
			"Network": c.Metadata.Network,
			"Protocol": c.Metadata.Protocol,
		})
	}
	data, err := json.Marshal(items)
	if err != nil {
		return nil, err
	}
	out.ConnectionStatisticsJson = string(data)
	return out, nil
}

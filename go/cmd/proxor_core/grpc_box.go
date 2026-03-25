package main

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"strings"

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

	if in.Mode == gen.TestMode_UrlTest {
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
		// Latency
		out.Ms, err = speedtest.UrlTest(boxapi.CreateProxyHttpClient(i), in.Url, in.Timeout, speedtest.UrlTestStandard_RTT)
	} else if in.Mode == gen.TestMode_TcpPing {
		out.Ms, err = speedtest.TcpPing(in.Address, in.Timeout)
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
			} else if c.Metadata.ProcessInfo.AndroidPackageName != "" {
				process = c.Metadata.ProcessInfo.AndroidPackageName
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

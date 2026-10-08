package main

import (
	"fmt"
	"os"
	_ "unsafe"

	"grpc_server"

	"github.com/Ogstra/proxorlib/proxor_common"
	"github.com/sagernet/sing-box/constant"
	boxmain "proxor_core/boxmain"
	"proxor_core/machelper"
)

func main() {
	// macOS privileged helper (LaunchDaemon). Handled before the version banner so
	// the daemon log is clean; every other mode is untouched.
	if len(os.Args) > 1 && os.Args[1] == "helper" {
		os.Exit(machelper.Main(os.Args[2:]))
	}

	versionSBox := constant.Version
	if versionSBox == "unknown" {
		versionSBox = "1.13.13"
	}
	fmt.Printf("proxor-core %s (sing-box %s)\n", proxor_common.Version_proxor, versionSBox)

	// proxor_core
	if len(os.Args) > 1 && os.Args[1] == "proxor" {
		proxor_common.RunMode = proxor_common.RunMode_ProxorCore
		grpc_server.RunCore(setupCore, &server{})
		return
	}

	// sing-box
	boxmain.Main()
}

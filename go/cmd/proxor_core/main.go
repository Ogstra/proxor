package main

import (
	"fmt"
	"os"
	_ "unsafe"

	"grpc_server"

	"github.com/Ogstra/proxorlib/proxor_common"
	"github.com/sagernet/sing-box/constant"
	boxmain "proxor_core/boxmain"
)

func main() {
	fmt.Println("sing-box:", constant.Version, "Proxor:", proxor_common.Version_neko)
	fmt.Println()

	// proxor_core
	if len(os.Args) > 1 && os.Args[1] == "proxor" {
		proxor_common.RunMode = proxor_common.RunMode_ProxorCore
		grpc_server.RunCore(setupCore, &server{})
		return
	}

	// sing-box
	boxmain.Main()
}

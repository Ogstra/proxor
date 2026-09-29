//go:build darwin

package machelper

import (
	"net"
	"os"
	"os/exec"
)

// selfUninstall removes the helper: stops Tun and restores the proxy (Shutdown),
// deletes the plist, binary, support dir and socket, then asks launchd to boot
// the job out. launchctl bootout terminates this process with SIGTERM (Main's
// handler exits 0); if it returns anyway, exit 0 ourselves. KeepAlive is
// SuccessfulExit=false, so an exit status of 0 is never restarted.
//
// The listener is closed only when everything else is done: closing it earlier
// would make Serve return and let Main exit while the removal was half way.
func selfUninstall(srv *Server, l net.Listener) {
	srv.Shutdown()
	_ = os.Remove(LaunchDaemonPlistPath)
	_ = os.Remove(HelperBinaryPath)
	_ = os.RemoveAll(SupportDir)
	_ = os.Remove(SocketPath)
	_ = exec.Command("/bin/launchctl", "bootout", "system/"+Label).Run()
	_ = l.Close()
	os.Exit(0)
}

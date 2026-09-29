//go:build darwin

package machelper

import (
	"errors"
	"io/fs"
	"log"
	"net"
	"os"
	"os/signal"
	"path/filepath"
	"strings"
	"syscall"
	"time"

	"github.com/Ogstra/proxorlib/proxor_common"
	"github.com/sagernet/sing-box/constant"
)

// orphanCheckEvery is deliberately longer than the tracker's 10 minute spacing:
// with a 10 minute ticker, scheduling jitter could make two consecutive checks
// land just under 10 minutes apart and skip a count.
const orphanCheckEvery = 11 * time.Minute

// Main is the `proxor_core helper` entry point: the root LaunchDaemon. It
// returns the process exit status (0 after SIGTERM/SIGINT or self-removal, so
// launchd's KeepAlive{SuccessfulExit=false} does not restart it).
func Main(args []string) int {
	if os.Geteuid() != 0 {
		os.Stderr.WriteString("proxor_core helper must run as root (installed by Proxor as a LaunchDaemon)\n")
		return 1
	}
	logger := log.New(os.Stderr, "proxor-helper ", log.LstdFlags)

	// sing-box opens ./cache.db whenever a log writer is attached (the Tun log is streamed to the
	// GUI), and launchd starts daemons in "/", which is read-only. Work from the root-owned support
	// directory instead; uninstall removes it together with the cache.
	if err := os.Chdir(SupportDir); err != nil {
		logger.Printf("chdir %s: %v (Tun will fail to open its cache file)", SupportDir, err)
	}

	// Register first so a SIGTERM during startup still gets a clean exit.
	sigs := make(chan os.Signal, 2)
	signal.Notify(sigs, syscall.SIGTERM, syscall.SIGINT)

	allow := &fileAllowlist{path: filepath.Join(SupportDir, "allowed-uids")}
	pm := &ProxyManager{
		Cmd:   NetworkSetup{},
		Store: SnapshotStore{Path: filepath.Join(SupportDir, "proxy-snapshot.json")},
	}
	// A crash or reboot while the system proxy was ours leaves it pointing at a
	// dead port: put the user's original settings back before serving anyone.
	if err := pm.RecoverAtStart(); err != nil {
		logger.Printf("recover system proxy: %v", err)
	}

	_ = os.Remove(SocketPath)
	l, err := net.Listen("unix", SocketPath)
	if err != nil {
		logger.Printf("listen %s: %v", SocketPath, err)
		return 1
	}
	// World-connectable on purpose: access is decided per connection by
	// LOCAL_PEERCRED + the allowlist, not by file permissions.
	if err := os.Chmod(SocketPath, 0o666); err != nil {
		logger.Printf("chmod %s: %v", SocketPath, err)
		_ = l.Close()
		_ = os.Remove(SocketPath)
		return 1
	}

	singBox := constant.Version
	if singBox == "unknown" {
		singBox = "1.13.13"
	}
	srv := &Server{
		Tun:     NewBoxTunRunner(),
		Proxy:   pm,
		PeerUID: PeerUID,
		Allowed: allow.Allowed,
		Build:   proxor_common.Version_proxor,
		SingBox: singBox,
		Logf:    logger.Printf,
	}
	srv.OnUninstall = func() { go selfUninstall(srv, l) }

	go orphanLoop(srv, pm, logger, l)

	logger.Printf("started (proxor %s, sing-box %s)", srv.Build, srv.SingBox)
	serveDone := make(chan error, 1)
	go func() { serveDone <- srv.Serve(l) }()

	select {
	case sig := <-sigs:
		logger.Printf("received %v, shutting down", sig)
		srv.Shutdown()
		_ = l.Close()
		_ = os.Remove(SocketPath)
		return 0
	case err := <-serveDone:
		// Serve returns nil when the listener was closed. Self-removal closes it
		// only as its very last step, so the removal is already complete here.
		srv.Shutdown()
		_ = os.Remove(SocketPath)
		if err != nil {
			logger.Printf("serve: %v", err)
			return 1
		}
		return 0
	}
}

// orphanLoop removes the helper when the app it was installed for is gone
// (dragged to the Trash, `brew uninstall` without zap). It never acts while
// Tun or the system proxy are in use.
func orphanLoop(srv *Server, pm *ProxyManager, logger *log.Logger, l net.Listener) {
	var tracker OrphanTracker
	check := func() {
		path := readAppPath()
		if !tracker.ObserveApp(time.Now(), path, appPresent) {
			return
		}
		if srv.Tun.Running() || pm.Applied() {
			return // in use: try again at the next tick
		}
		logger.Printf("app %q is gone (%d checks), removing helper", path, tracker.Misses())
		selfUninstall(srv, l)
	}
	check()
	t := time.NewTicker(orphanCheckEvery)
	defer t.Stop()
	for range t.C {
		check()
	}
}

// readAppPath returns the recorded app path, or "" when it cannot be read
// (which the tracker treats as "keep the helper").
func readAppPath() string {
	data, err := os.ReadFile(filepath.Join(SupportDir, "app-path"))
	if err != nil {
		return ""
	}
	line, _, _ := strings.Cut(string(data), "\n")
	return strings.TrimSpace(line)
}

// appPresent reports whether the path exists. Only a definite "does not
// exist" counts as missing; any other error (permissions, I/O) is treated as
// present so the helper is never removed on an inconclusive check.
func appPresent(path string) bool {
	_, err := os.Stat(path)
	return err == nil || !errors.Is(err, fs.ErrNotExist)
}

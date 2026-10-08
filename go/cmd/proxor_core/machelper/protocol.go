// Package machelper holds the macOS privileged-helper contract: the wire
// protocol shared with the GUI client and the deny-by-default validator for the
// Tun config the GUI sends. Everything in files without a _darwin suffix builds
// on every platform so CI on Linux runs the tests.
package machelper

import (
	"bufio"
	"encoding/json"
	"errors"
	"io"
)

// ProtocolVersion is bumped only when the wire contract changes incompatibly.
// The C++ client mirrors this constant; a mismatch forces a helper reinstall.
const ProtocolVersion = 1

const Label = "io.github.Ogstra.Proxor.helper"
const SocketPath = "/var/run/io.github.Ogstra.Proxor.helper.sock"
const HelperBinaryPath = "/Library/PrivilegedHelperTools/io.github.Ogstra.Proxor.helper"
const LaunchDaemonPlistPath = "/Library/LaunchDaemons/io.github.Ogstra.Proxor.helper.plist"
const SupportDir = "/Library/Application Support/Proxor" // allowed-uids, app-path, proxy-snapshot.json
const MaxLineBytes = 1 << 20

const (
	CmdHello           = "hello"
	CmdStatus          = "status"
	CmdTunStart        = "tun_start"
	CmdTunStop         = "tun_stop"
	CmdSysproxyApply   = "sysproxy_apply"
	CmdSysproxyRestore = "sysproxy_restore"
	CmdUninstall       = "uninstall"
)

const (
	EventTunReady   = "tun_ready"
	EventTunStopped = "tun_stopped"
	EventLog        = "log"
)

type Request struct {
	ID        int64    `json:"id"`
	Cmd       string   `json:"cmd"`
	Protocol  int      `json:"protocol,omitempty"`  // hello
	Config    string   `json:"config,omitempty"`    // tun_start: rendered sing-box JSON text
	SocksPort int      `json:"socksPort,omitempty"` // tun_start: user core mixed port to wait for
	Port      int      `json:"port,omitempty"`      // sysproxy_apply
	Bypass    []string `json:"bypass,omitempty"`    // sysproxy_apply
}

type Response struct {
	ID           int64    `json:"id"`
	OK           bool     `json:"ok"`
	Error        string   `json:"error,omitempty"`
	Protocol     int      `json:"protocol,omitempty"`
	Build        string   `json:"build,omitempty"`
	SingBox      string   `json:"singbox,omitempty"`
	UID          uint32   `json:"uid,omitempty"`
	TunRunning   bool     `json:"tunRunning,omitempty"`
	ProxyApplied bool     `json:"proxyApplied,omitempty"`
	Applied      []string `json:"applied,omitempty"` // sysproxy_apply: services changed
	Failed       []string `json:"failed,omitempty"`  // sysproxy_apply: "<service>: <error>"
}

type Event struct {
	Event  string `json:"event"`
	Line   string `json:"line,omitempty"`
	Reason string `json:"reason,omitempty"`
}

var ErrLineTooLong = errors.New("request line exceeds 1 MiB")

// ReadRequest reads one '\n'-terminated JSON line. It never buffers more than
// MaxLineBytes: an oversized line yields ErrLineTooLong and no partial request.
func ReadRequest(r *bufio.Reader) (Request, error) {
	var line []byte
	for {
		chunk, err := r.ReadSlice('\n')
		content := len(line) + len(chunk)
		if err == nil {
			content-- // the terminating '\n' is not part of the line
		}
		if content > MaxLineBytes {
			return Request{}, ErrLineTooLong
		}
		line = append(line, chunk...)
		if err == nil {
			line = line[:len(line)-1] // drop '\n'
			break
		}
		if errors.Is(err, bufio.ErrBufferFull) {
			continue
		}
		if errors.Is(err, io.EOF) && len(line) > 0 {
			return Request{}, io.ErrUnexpectedEOF
		}
		return Request{}, err
	}
	var req Request
	if err := json.Unmarshal(line, &req); err != nil {
		return Request{}, err
	}
	return req, nil
}

// WriteMessage writes v as a single JSON object followed by '\n' in one Write call.
func WriteMessage(w io.Writer, v any) error {
	data, err := json.Marshal(v)
	if err != nil {
		return err
	}
	data = append(data, '\n')
	_, err = w.Write(data)
	return err
}

// TunRunner is implemented by later plans; the server depends only on this.
type TunRunner interface {
	// Start validates+checks config synchronously (error => nothing started), then asynchronously
	// waits for 127.0.0.1:socksPort, starts the tun instance and reports via
	// emit(EventTunReady / EventTunStopped{Reason}) and emit(EventLog{Line}).
	Start(config []byte, socksPort int, emit func(Event)) error
	Stop() error
	Running() bool
}

// ProxyBackend is implemented by later plans; the server depends only on this.
type ProxyBackend interface {
	Apply(port int, bypass []string) (applied []string, failed []string, err error)
	Restore() error
	Applied() bool
}

// Commander (networksetup argv runner) is defined by plan 50-02 in sysproxy_manager.go, not here.

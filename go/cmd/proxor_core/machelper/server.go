package machelper

import (
	"bufio"
	"encoding/json"
	"errors"
	"fmt"
	"net"
	"sync"
	"time"
)

// The server depends only on the two interfaces; the compile-time assertion
// keeps the real system proxy manager honest.
var _ ProxyBackend = (*ProxyManager)(nil)

const defaultHandshakeTimeout = 5 * time.Second

// writeTimeout bounds a single reply/event write so a client that stops
// reading cannot wedge the Tun log stream or the request loop forever.
const writeTimeout = 5 * time.Second

// Server is the helper's request loop. Everything it can do is the fixed
// command set below; a client can never name a path, a binary, a network
// service or a command line.
type Server struct {
	Tun         TunRunner
	Proxy       ProxyBackend
	PeerUID     func(net.Conn) (uint32, error) // darwin: LOCAL_PEERCRED (plan 50-06); tests inject
	Allowed     func(uid uint32) bool          // uid 0 is always allowed by Serve itself
	Build       string
	SingBox     string
	OnUninstall func() // plan 50-08: self-uninstall (removes files, bootout)

	HandshakeTimeout time.Duration // default 5 s
	Logf             func(format string, args ...any)

	mu   sync.Mutex // serialises tun_* / sysproxy_* / uninstall across connections
	down bool       // set by Shutdown; guarded by mu
}

func (s *Server) logf(format string, args ...any) {
	if s.Logf != nil {
		s.Logf(format, args...)
	}
}

// Serve accepts connections until l is closed. Each connection gets its own
// goroutine. It returns nil when the listener was closed.
func (s *Server) Serve(l net.Listener) error {
	if s.PeerUID == nil {
		// Fail closed: without a peer credential check nobody may connect.
		return errors.New("machelper: Server.PeerUID is required")
	}
	for {
		c, err := l.Accept()
		if err != nil {
			if errors.Is(err, net.ErrClosed) {
				return nil
			}
			var ne net.Error
			if errors.As(err, &ne) && ne.Timeout() {
				continue
			}
			return err
		}
		go s.handle(c)
	}
}

// Shutdown stops Tun and restores the system proxy (SIGTERM path).
func (s *Server) Shutdown() {
	s.mu.Lock()
	s.down = true
	s.stopTunLocked()
	s.restoreProxyLocked()
	s.mu.Unlock()
}

// stopTunLocked and restoreProxyLocked must be called with s.mu held.
func (s *Server) stopTunLocked() error {
	if s.Tun == nil || !s.Tun.Running() {
		return nil
	}
	return s.Tun.Stop()
}

func (s *Server) restoreProxyLocked() error {
	if s.Proxy == nil {
		return nil
	}
	return s.Proxy.Restore()
}

// session is one authenticated-or-not client connection.
type session struct {
	srv  *Server
	conn net.Conn
	uid  uint32
	wmu  sync.Mutex // replies and events come from different goroutines
}

func (c *session) write(v any) error {
	c.wmu.Lock()
	defer c.wmu.Unlock()
	_ = c.conn.SetWriteDeadline(time.Now().Add(writeTimeout))
	return WriteMessage(c.conn, v)
}

func (s *Server) handle(nc net.Conn) {
	defer nc.Close()

	uid, err := s.PeerUID(nc)
	if err != nil {
		s.logf("reject connection: peer credential unavailable: %v", err)
		return
	}
	if uid != 0 && (s.Allowed == nil || !s.Allowed(uid)) {
		s.logf("reject connection from uid %d: not allowed", uid)
		return
	}

	c := &session{srv: s, conn: nc, uid: uid}
	br := bufio.NewReader(nc)

	timeout := s.HandshakeTimeout
	if timeout <= 0 {
		timeout = defaultHandshakeTimeout
	}
	_ = nc.SetReadDeadline(time.Now().Add(timeout))
	req, err := ReadRequest(br)
	if err != nil || req.Cmd != CmdHello {
		s.logf("uid %d: handshake failed (first message must be hello): %v", uid, err)
		return
	}
	if req.Protocol != ProtocolVersion {
		s.logf("uid %d: protocol mismatch (client %d, helper %d)", uid, req.Protocol, ProtocolVersion)
		_ = c.write(Response{ID: req.ID, Error: "protocol mismatch", Protocol: ProtocolVersion})
		return
	}
	_ = nc.SetReadDeadline(time.Time{})
	if err := c.write(c.helloReply(req.ID)); err != nil {
		return
	}
	s.logf("uid %d: connected", uid)
	defer s.logf("uid %d: disconnected", uid)

	for {
		req, err := ReadRequest(br)
		if err != nil {
			var syn *json.SyntaxError
			var typ *json.UnmarshalTypeError
			if errors.As(err, &syn) || errors.As(err, &typ) {
				// The line was fully consumed; the stream is still in sync.
				if c.write(Response{Error: "bad request"}) != nil {
					return
				}
				continue
			}
			return // EOF, oversize line, network error
		}
		if !c.dispatch(req) {
			return
		}
	}
}

func (c *session) helloReply(id int64) Response {
	return Response{
		ID: id, OK: true, Protocol: ProtocolVersion,
		Build: c.srv.Build, SingBox: c.srv.SingBox, UID: c.uid,
	}
}

func failure(id int64, msg string) Response { return Response{ID: id, Error: msg} }

// dispatch runs one command and writes its reply. It returns false when the
// connection should be closed.
func (c *session) dispatch(req Request) bool {
	s := c.srv
	// The config body is never logged.
	s.logf("uid %d: %s (id %d)", c.uid, req.Cmd, req.ID)

	var resp Response
	switch req.Cmd {
	case CmdHello:
		resp = c.helloReply(req.ID)
	case CmdStatus:
		resp = Response{
			ID: req.ID, OK: true,
			TunRunning:   s.Tun != nil && s.Tun.Running(),
			ProxyApplied: s.Proxy != nil && s.Proxy.Applied(),
		}
	case CmdTunStart:
		resp = c.tunStart(req)
	case CmdTunStop:
		resp = c.tunStop(req)
	case CmdSysproxyApply:
		resp = c.sysproxyApply(req)
	case CmdSysproxyRestore:
		resp = c.sysproxyRestore(req)
	case CmdUninstall:
		return c.uninstall(req)
	default:
		resp = failure(req.ID, "unknown command")
	}
	return c.write(resp) == nil
}

func validPort(p int) bool { return p >= 1 && p <= 65535 }

func (c *session) tunStart(req Request) Response {
	s := c.srv
	if !validPort(req.SocksPort) {
		return failure(req.ID, fmt.Sprintf("invalid socksPort %d", req.SocksPort))
	}
	if req.Config == "" {
		return failure(req.ID, "config is required")
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	if s.down {
		return failure(req.ID, "helper is shutting down")
	}
	emit := func(ev Event) { _ = c.write(ev) }
	if err := s.Tun.Start([]byte(req.Config), req.SocksPort, emit); err != nil {
		return failure(req.ID, err.Error())
	}
	return Response{ID: req.ID, OK: true}
}

func (c *session) tunStop(req Request) Response {
	s := c.srv
	s.mu.Lock()
	defer s.mu.Unlock()
	if err := s.Tun.Stop(); err != nil {
		return failure(req.ID, err.Error())
	}
	return Response{ID: req.ID, OK: true}
}

func (c *session) sysproxyApply(req Request) Response {
	s := c.srv
	if !validPort(req.Port) {
		return failure(req.ID, fmt.Sprintf("invalid port %d", req.Port))
	}
	if err := ValidateBypass(req.Bypass); err != nil {
		return failure(req.ID, err.Error())
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	if s.down {
		return failure(req.ID, "helper is shutting down")
	}
	applied, failed, err := s.Proxy.Apply(req.Port, req.Bypass)
	if err != nil {
		return Response{ID: req.ID, Error: err.Error(), Applied: applied, Failed: failed}
	}
	return Response{ID: req.ID, OK: true, Applied: applied, Failed: failed}
}

func (c *session) sysproxyRestore(req Request) Response {
	s := c.srv
	s.mu.Lock()
	defer s.mu.Unlock()
	if err := s.Proxy.Restore(); err != nil {
		return failure(req.ID, err.Error())
	}
	return Response{ID: req.ID, OK: true}
}

// uninstall stops Tun and restores the proxy, answers ok, and only then hands
// over to OnUninstall (which removes the helper itself). If the proxy cannot
// be restored the uninstall is refused: removing the helper would also remove
// the snapshot needed to repair it.
func (c *session) uninstall(req Request) bool {
	s := c.srv
	if s.OnUninstall == nil {
		return c.write(failure(req.ID, "uninstall unavailable")) == nil
	}
	s.mu.Lock()
	var err error
	if e := s.stopTunLocked(); e != nil {
		err = fmt.Errorf("stop tun: %w", e)
	}
	if e := s.restoreProxyLocked(); e != nil {
		err = errors.Join(err, fmt.Errorf("restore system proxy: %w", e))
	}
	if err == nil {
		s.down = true
	}
	s.mu.Unlock()
	if err != nil {
		return c.write(failure(req.ID, err.Error())) == nil
	}
	_ = c.write(Response{ID: req.ID, OK: true})
	s.OnUninstall() // not under s.mu: it may call Shutdown
	return false
}

package machelper

import (
	"net"
	"sync"
)

// A lease is one authenticated client connection. Whatever that connection
// starts (the Tun instance, the applied system proxy) is owned by it and is
// torn down when the connection ends, however it ends (quit, crash, kill -9).
// This is the dead-man switch: the GUI needs no bookkeeping and a dead GUI can
// never leave Tun up or the system proxy pointing at a dead port.
type lease struct {
	id    uint64
	write func(v any) error // serialised writes to the owning connection
}

// leaseTable records who owns what. Its mutex is separate from Server.mu on
// purpose: TunRunner.Start may call emit synchronously while the request
// loop holds Server.mu, and emit needs the table.
type leaseTable struct {
	mu         sync.Mutex
	nextID     uint64
	tunOwner   *lease
	tunGen     uint64 // bumped whenever a Tun instance is superseded; stale emits are dropped
	proxyOwner *lease
	conns      map[net.Conn]struct{}
	down       bool // Shutdown ran: no new connections
}

func (t *leaseTable) newLease(write func(any) error) *lease {
	t.mu.Lock()
	defer t.mu.Unlock()
	t.nextID++
	return &lease{id: t.nextID, write: write}
}

// addConn registers a raw connection so Shutdown can drop it. It reports
// false when the helper is already shutting down.
func (t *leaseTable) addConn(c net.Conn) bool {
	t.mu.Lock()
	defer t.mu.Unlock()
	if t.down {
		return false
	}
	if t.conns == nil {
		t.conns = make(map[net.Conn]struct{})
	}
	t.conns[c] = struct{}{}
	return true
}

func (t *leaseTable) removeConn(c net.Conn) {
	t.mu.Lock()
	delete(t.conns, c)
	t.mu.Unlock()
}

// shutdown refuses further connections and returns the live ones.
func (t *leaseTable) shutdown() []net.Conn {
	t.mu.Lock()
	defer t.mu.Unlock()
	t.down = true
	out := make([]net.Conn, 0, len(t.conns))
	for c := range t.conns {
		out = append(out, c)
	}
	return out
}

// takeTun drops the current Tun owner and invalidates the running instance's
// event stream. The previous owner (possibly nil) is returned.
func (t *leaseTable) takeTun() *lease {
	t.mu.Lock()
	defer t.mu.Unlock()
	prev := t.tunOwner
	t.tunOwner = nil
	t.tunGen++
	return prev
}

// claimTun makes l the owner of the instance about to start and returns the
// generation its emit func must carry.
func (t *leaseTable) claimTun(l *lease) uint64 {
	t.mu.Lock()
	defer t.mu.Unlock()
	t.tunOwner = l
	return t.tunGen
}

// dropTun clears ownership if l still holds it (start failed / tun_stop).
func (t *leaseTable) dropTun(l *lease) {
	t.mu.Lock()
	if t.tunOwner == l {
		t.tunOwner = nil
	}
	t.mu.Unlock()
}

// clearTun ends Tun ownership after a successful explicit tun_stop. The
// generation is kept so the instance can still report its own tun_stopped.
func (t *leaseTable) clearTun() {
	t.mu.Lock()
	t.tunOwner = nil
	t.mu.Unlock()
}

func (t *leaseTable) setProxyOwner(l *lease) {
	t.mu.Lock()
	t.proxyOwner = l
	t.mu.Unlock()
}

// clearProxy ends proxy ownership after a successful restore.
func (t *leaseTable) clearProxy() { t.setProxyOwner(nil) }

// route delivers ev to l if the emitting instance is still the current one.
// A tun_stopped event also ends l's ownership so nothing is stopped twice.
func (t *leaseTable) route(l *lease, gen uint64, ev Event) {
	t.mu.Lock()
	current := t.tunGen == gen
	if current && ev.Event == EventTunStopped && t.tunOwner == l {
		t.tunOwner = nil
	}
	t.mu.Unlock()
	if current {
		_ = l.write(ev)
	}
}

// release ends l: whatever it owned is stopped or restored. Must be called
// with Server.mu held (so it cannot interleave with another tun_*/sysproxy_*).
func (s *Server) releaseLocked(l *lease) {
	t := &s.leases
	t.mu.Lock()
	stopTun := t.tunOwner == l
	if stopTun {
		t.tunOwner = nil
		t.tunGen++
	}
	restoreProxy := t.proxyOwner == l
	if restoreProxy {
		t.proxyOwner = nil
	}
	t.mu.Unlock()

	if stopTun {
		s.logf("lease %d closed: stopping tun", l.id)
		if err := s.Tun.Stop(); err != nil {
			s.logf("lease %d: stop tun: %v", l.id, err)
		}
	}
	if restoreProxy {
		s.logf("lease %d closed: restoring system proxy", l.id)
		if err := s.Proxy.Restore(); err != nil {
			s.logf("lease %d: restore system proxy: %v", l.id, err)
		}
	}
}

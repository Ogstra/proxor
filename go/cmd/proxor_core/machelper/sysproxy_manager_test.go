package machelper

import (
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"reflect"
	"strconv"
	"strings"
	"sync"
	"testing"
	"time"
)

// simSvc is one simulated network service, mirroring what networksetup keeps.
type simSvc struct {
	device         string
	web, secure    ProxyState
	socks          ProxyState
	bypass         []string
	autoURL        string
	autoURLEnabled bool
	discovery      bool
}

type simFailure struct {
	out string
	err error
}

// simNet is a stateful fake of /usr/sbin/networksetup behind the Commander
// interface: getters return the current state, setters mutate it, so
// snapshot/apply/restore can be checked end to end without touching a Mac.
type simNet struct {
	mu       sync.Mutex
	order    []string
	svcs     map[string]*simSvc
	disabled map[string]bool
	calls    [][]string
	onSet    func(args []string) // runs before the first -set* call takes effect
	setSeen  bool
	fail     func(args []string) *simFailure

	// concurrency instrumentation (all guarded by mu)
	delay       time.Duration         // slept per Run, outside the state lock
	inflight    int                   // Run calls currently executing
	maxInflight int                   // high-water mark of inflight
	svcInflight map[string]int        // per-service Run calls currently executing
	overlap     []string              // services that saw two overlapping Run calls
	writes      map[string][][]string // per-service -set* argv, in start order
}

// resetLog clears the call and write logs and the concurrency counters.
func (n *simNet) resetLog() {
	n.mu.Lock()
	defer n.mu.Unlock()
	n.calls = nil
	n.writes = map[string][][]string{}
	n.overlap = nil
	n.maxInflight = 0
}

// failFirstWrite makes the first -set* call for service fail once.
func (n *simNet) failFirstWrite(service string, f simFailure) {
	n.mu.Lock()
	defer n.mu.Unlock()
	done := false
	n.fail = func(args []string) *simFailure {
		if !done && strings.HasPrefix(args[0], "-set") && len(args) > 1 && args[1] == service {
			done = true
			return &f
		}
		return nil
	}
}

func newSimNet4() *simNet {
	n := newSimNet()
	n.add("Thunderbolt Bridge", "bridge0", &simSvc{bypass: []string{"foo.example"}})
	n.add("iPhone USB", "en7", &simSvc{
		web:            ProxyState{Enabled: true, Server: "9.9.9.9", Port: 8080},
		autoURL:        "http://pac.example/x.pac",
		autoURLEnabled: true,
	})
	return n
}

func newSimNet() *simNet {
	n := &simNet{svcs: map[string]*simSvc{}, disabled: map[string]bool{}}
	n.add("Wi-Fi", "en0", &simSvc{
		secure:         ProxyState{Enabled: true, Server: "10.0.0.1", Port: 3128},
		bypass:         []string{"*.local", "169.254/16"},
		autoURL:        "http://wpad.example/proxy.pac",
		autoURLEnabled: true,
		discovery:      true,
	})
	n.add("USB 10/100/1000 LAN", "en10", &simSvc{})
	n.add("Happ", "", &simSvc{}) // Network-Extension VPN: empty Device
	return n
}

func (n *simNet) add(name, device string, s *simSvc) {
	s.device = device
	n.order = append(n.order, name)
	n.svcs[name] = s
}

func (n *simNet) clone() map[string]simSvc {
	out := map[string]simSvc{}
	for k, v := range n.svcs {
		c := *v
		c.bypass = append([]string(nil), v.bypass...)
		out[k] = c
	}
	return out
}

func fmtProxy(p ProxyState) string {
	yn := "No"
	if p.Enabled {
		yn = "Yes"
	}
	return fmt.Sprintf("Enabled: %s\nServer: %s\nPort: %d\nAuthenticated Proxy Enabled: 0\n", yn, p.Server, p.Port)
}

func (n *simNet) countPrefix(prefix string) int {
	c := 0
	for _, a := range n.calls {
		if strings.HasPrefix(a[0], prefix) {
			c++
		}
	}
	return c
}

func (n *simNet) touched(service string) bool {
	for _, a := range n.calls {
		if len(a) > 1 && a[1] == service {
			return true
		}
	}
	return false
}

func (n *simNet) Run(args ...string) (string, error) {
	svc := ""
	if len(args) > 1 {
		svc = args[1]
	}
	n.mu.Lock()
	n.inflight++
	if n.inflight > n.maxInflight {
		n.maxInflight = n.inflight
	}
	if n.svcInflight == nil {
		n.svcInflight = map[string]int{}
	}
	isWrite := len(args) > 1 && strings.HasPrefix(args[0], "-set")
	if isWrite { // reads of one service run concurrently on purpose; writes must not
		if n.svcInflight[svc]++; n.svcInflight[svc] > 1 {
			n.overlap = append(n.overlap, svc)
		}
		if n.writes == nil {
			n.writes = map[string][][]string{}
		}
		n.writes[svc] = append(n.writes[svc], append([]string(nil), args...))
	}
	delay := n.delay
	n.mu.Unlock()

	if delay > 0 {
		time.Sleep(delay) // a slow networksetup process; the state lock is NOT held
	}

	n.mu.Lock()
	defer func() {
		n.inflight--
		if isWrite {
			n.svcInflight[svc]--
		}
		n.mu.Unlock()
	}()
	return n.exec(args)
}

// exec runs one command against the simulated state; n.mu is held.
func (n *simNet) exec(args []string) (string, error) {
	n.calls = append(n.calls, append([]string(nil), args...))
	if len(args) == 0 {
		return "", errors.New("no args")
	}
	if strings.HasPrefix(args[0], "-set") && !n.setSeen {
		n.setSeen = true
		if n.onSet != nil {
			n.onSet(args)
		}
	}
	if n.fail != nil {
		if f := n.fail(args); f != nil {
			return f.out, f.err
		}
	}
	switch args[0] {
	case "-listnetworkserviceorder":
		var b strings.Builder
		b.WriteString("An asterisk (*) denotes that a network service is disabled.\n")
		for i, name := range n.order {
			fmt.Fprintf(&b, "(%d) %s\n(Hardware Port: %s, Device: %s)\n\n", i+1, name, name, n.svcs[name].device)
		}
		return b.String(), nil
	case "-listallnetworkservices":
		var b strings.Builder
		b.WriteString("An asterisk (*) denotes that a network service is disabled.\n")
		for _, name := range n.order {
			if n.disabled[name] {
				b.WriteString("*")
			}
			b.WriteString(name + "\n")
		}
		return b.String(), nil
	}
	if len(args) < 2 {
		return "", errors.New("missing service")
	}
	s, ok := n.svcs[args[1]]
	if !ok {
		return "** Error: Unable to find item in network database.\n", errors.New("exit status 1")
	}
	proxy := func(cmd string) *ProxyState {
		switch {
		case strings.HasPrefix(cmd, "-getweb"), strings.HasPrefix(cmd, "-setweb"):
			return &s.web
		case strings.HasPrefix(cmd, "-getsecureweb"), strings.HasPrefix(cmd, "-setsecureweb"):
			return &s.secure
		default:
			return &s.socks
		}
	}
	switch args[0] {
	case "-getwebproxy", "-getsecurewebproxy", "-getsocksfirewallproxy":
		return fmtProxy(*proxy(args[0])), nil
	case "-getproxybypassdomains":
		if len(s.bypass) == 0 {
			return "There aren't any bypass domains set on " + args[1] + ".\n", nil
		}
		return strings.Join(s.bypass, "\n") + "\n", nil
	case "-getautoproxyurl":
		url, yn := "(null)", "No"
		if s.autoURL != "" {
			url = s.autoURL
		}
		if s.autoURLEnabled {
			yn = "Yes"
		}
		return fmt.Sprintf("URL: %s\nEnabled: %s\n", url, yn), nil
	case "-getproxyautodiscovery":
		if s.discovery {
			return "Auto Proxy Discovery: On\n", nil
		}
		return "Auto Proxy Discovery: Off\n", nil
	case "-setwebproxy", "-setsecurewebproxy", "-setsocksfirewallproxy":
		if len(args) != 4 {
			return "", errors.New("bad arity")
		}
		port, err := strconv.Atoi(args[3])
		if err != nil {
			return "", err
		}
		p := proxy(args[0])
		p.Server, p.Port, p.Enabled = args[2], port, true // real networksetup enables on set
		return "", nil
	case "-setwebproxystate", "-setsecurewebproxystate", "-setsocksfirewallproxystate":
		proxy(args[0]).Enabled = args[2] == "on"
		return "", nil
	case "-setproxybypassdomains":
		if len(args) == 3 && args[2] == "Empty" {
			s.bypass = nil
		} else {
			s.bypass = append([]string(nil), args[2:]...)
		}
		return "", nil
	case "-setautoproxyurl":
		s.autoURL, s.autoURLEnabled = args[2], true // real networksetup enables on set
		return "", nil
	case "-setautoproxystate":
		s.autoURLEnabled = args[2] == "on"
		return "", nil
	case "-setproxyautodiscovery":
		s.discovery = args[2] == "on"
		return "", nil
	}
	return "", fmt.Errorf("unknown command %q", args[0])
}

// visibleEqual compares what the user sees in Network settings. When the
// recorded server was empty the restore only turns the entry off, so a
// disabled entry's leftover server/port is not compared.
func visibleEqual(orig, got simSvc) bool {
	one := func(a, b ProxyState) bool {
		if a.Enabled != b.Enabled {
			return false
		}
		if a.Enabled || a.Server != "" {
			return a.Server == b.Server && a.Port == b.Port
		}
		return true
	}
	return one(orig.web, got.web) && one(orig.secure, got.secure) && one(orig.socks, got.socks) &&
		reflect.DeepEqual(append([]string(nil), orig.bypass...), append([]string(nil), got.bypass...)) &&
		orig.autoURL == got.autoURL && orig.autoURLEnabled == got.autoURLEnabled &&
		orig.discovery == got.discovery
}

func assertRestored(t *testing.T, orig, got map[string]simSvc) {
	t.Helper()
	for name, o := range orig {
		if !visibleEqual(o, got[name]) {
			t.Errorf("service %q not restored exactly:\n orig: %+v\n  got: %+v", name, o, got[name])
		}
	}
}

func newManager(t *testing.T, n *simNet) (*ProxyManager, SnapshotStore) {
	t.Helper()
	store := SnapshotStore{Path: filepath.Join(t.TempDir(), "proxy-snapshot.json")}
	return &ProxyManager{Cmd: n, Store: store}, store
}

func TestProxyManagerApplyPersistsBeforeWrite(t *testing.T) {
	n := newSimNet()
	m, store := newManager(t, n)
	var seen *Snapshot
	var loadErr error
	n.onSet = func(args []string) {
		seen, loadErr = store.Load()
	}
	applied, failed, err := m.Apply(2080, []string{"127.0.0.1", "localhost"})
	if err != nil {
		t.Fatal(err)
	}
	if loadErr != nil || seen == nil {
		t.Fatalf("snapshot not on disk before the first -set call: %v %v", seen, loadErr)
	}
	if !seen.Applied || seen.Port != 2080 || len(seen.Services) != 2 {
		t.Fatalf("persisted snapshot = %+v", seen)
	}
	if got := seen.Services[0]; got.Name != "Wi-Fi" || got.Secure.Server != "10.0.0.1" || !got.AutoURLEnabled || !got.AutoDiscovery {
		t.Fatalf("persisted snapshot does not hold the ORIGINAL settings: %+v", got)
	}
	if !reflect.DeepEqual(applied, []string{"Wi-Fi", "USB 10/100/1000 LAN"}) || len(failed) != 0 {
		t.Fatalf("applied=%v failed=%v", applied, failed)
	}
	if n.touched("Happ") {
		t.Fatal("NE VPN service (empty Device) must never be touched")
	}
	if !m.Applied() {
		t.Fatal("Applied() must be true")
	}
	// the settings really point at us
	wifi := n.svcs["Wi-Fi"]
	if wifi.web != (ProxyState{Enabled: true, Server: "127.0.0.1", Port: 2080}) || wifi.autoURLEnabled || wifi.discovery {
		t.Fatalf("wifi after apply = %+v", wifi)
	}
	if !reflect.DeepEqual(wifi.bypass, []string{"127.0.0.1", "localhost"}) {
		t.Fatalf("bypass = %v", wifi.bypass)
	}
}

func TestProxyManagerApplyEmptyBypassUsesDefault(t *testing.T) {
	n := newSimNet()
	m, _ := newManager(t, n)
	if _, _, err := m.Apply(2080, nil); err != nil {
		t.Fatal(err)
	}
	if !reflect.DeepEqual(n.svcs["USB 10/100/1000 LAN"].bypass, DefaultBypass()) {
		t.Fatalf("bypass = %v", n.svcs["USB 10/100/1000 LAN"].bypass)
	}
}

func TestProxyManagerApplyPartial(t *testing.T) {
	n := newSimNet()
	n.fail = func(args []string) *simFailure {
		if args[0] == "-setwebproxy" && args[1] == "USB 10/100/1000 LAN" {
			return &simFailure{out: "** Error: Command requires admin privileges.\n", err: errors.New("exit status 14")}
		}
		return nil
	}
	m, _ := newManager(t, n)
	applied, failed, err := m.Apply(2080, nil)
	if err != nil {
		t.Fatalf("partial failure must not be an error: %v", err)
	}
	if !reflect.DeepEqual(applied, []string{"Wi-Fi"}) {
		t.Fatalf("applied = %v", applied)
	}
	if len(failed) != 1 || !strings.HasPrefix(failed[0], "USB 10/100/1000 LAN: ") || !strings.Contains(failed[0], "admin privileges") {
		t.Fatalf("failed = %q", failed)
	}
	if !m.Applied() {
		t.Fatal("still applied for the services that worked")
	}
}

func TestProxyManagerApplyTreatsErrorOutputAsFailure(t *testing.T) {
	n := newSimNet()
	n.fail = func(args []string) *simFailure {
		if args[0] == "-setsocksfirewallproxy" && args[1] == "Wi-Fi" {
			return &simFailure{out: "** Error: nope.\n"} // exit status 0 but error text
		}
		return nil
	}
	m, _ := newManager(t, n)
	applied, failed, err := m.Apply(2080, nil)
	if err != nil {
		t.Fatal(err)
	}
	if !reflect.DeepEqual(applied, []string{"USB 10/100/1000 LAN"}) || len(failed) != 1 || !strings.HasPrefix(failed[0], "Wi-Fi: ") {
		t.Fatalf("applied=%v failed=%q", applied, failed)
	}
}

func TestProxyManagerApplyAllFailRollsBack(t *testing.T) {
	n := newSimNet()
	orig := n.clone()
	n.fail = func(args []string) *simFailure {
		// the very last write of every service fails, after earlier writes went through
		if args[0] == "-setproxybypassdomains" && len(args) > 2 && args[2] != "Empty" && args[2] != "*.local" {
			return &simFailure{out: "** Error: denied\n", err: errors.New("exit status 1")}
		}
		return nil
	}
	m, store := newManager(t, n)
	applied, failed, err := m.Apply(2080, []string{"127.0.0.1"})
	if err == nil {
		t.Fatalf("all services failing must be an error (applied=%v failed=%v)", applied, failed)
	}
	if len(applied) != 0 || len(failed) != 2 {
		t.Fatalf("applied=%v failed=%v", applied, failed)
	}
	if m.Applied() {
		t.Fatal("must not report applied")
	}
	if snap, _ := store.Load(); snap != nil {
		t.Fatalf("store must be cleared after rollback, got %+v", snap)
	}
	assertRestored(t, orig, n.clone())
}

func TestProxyManagerRestore(t *testing.T) {
	n := newSimNet()
	orig := n.clone()
	m, store := newManager(t, n)
	if _, _, err := m.Apply(2080, nil); err != nil {
		t.Fatal(err)
	}
	if err := m.Restore(); err != nil {
		t.Fatal(err)
	}
	assertRestored(t, orig, n.clone())
	if m.Applied() {
		t.Fatal("Applied() must be false after Restore")
	}
	if snap, err := store.Load(); snap != nil || err != nil {
		t.Fatalf("store not cleared: %+v %v", snap, err)
	}
	if n.touched("Happ") {
		t.Fatal("NE VPN service touched")
	}
	before := len(n.calls)
	if err := m.Restore(); err != nil {
		t.Fatalf("Restore with nothing applied must be a no-op: %v", err)
	}
	if len(n.calls) != before {
		t.Fatal("no-op Restore must not call networksetup")
	}
}

func TestProxyManagerRestoreFailureKeepsSnapshotForRetry(t *testing.T) {
	n := newSimNet()
	orig := n.clone()
	m, store := newManager(t, n)
	if _, _, err := m.Apply(2080, nil); err != nil {
		t.Fatal(err)
	}
	broken := true
	n.fail = func(args []string) *simFailure {
		if broken && args[0] == "-setautoproxystate" && args[1] == "Wi-Fi" {
			return &simFailure{out: "** Error: busy\n", err: errors.New("exit status 1")}
		}
		return nil
	}
	if err := m.Restore(); err == nil {
		t.Fatal("restore failure must be reported")
	}
	if !m.Applied() {
		t.Fatal("still applied so the restore can be retried")
	}
	snap, _ := store.Load()
	if snap == nil || !snap.Applied || len(snap.Services) != 1 || snap.Services[0].Name != "Wi-Fi" {
		t.Fatalf("only the service that failed to restore should stay recorded, got %+v", snap)
	}
	broken = false
	if err := m.Restore(); err != nil {
		t.Fatal(err)
	}
	assertRestored(t, orig, n.clone())
	if m.Applied() {
		t.Fatal("applied after successful retry")
	}
}

func TestRecovery(t *testing.T) {
	n := newSimNet()
	orig := n.clone()
	m1, store := newManager(t, n)
	if _, _, err := m1.Apply(2080, nil); err != nil {
		t.Fatal(err)
	}
	// helper "crashes": a brand new manager over the same store recovers
	m2 := &ProxyManager{Cmd: n, Store: store}
	if m2.Applied() {
		t.Fatal("fresh manager starts idle")
	}
	if err := m2.RecoverAtStart(); err != nil {
		t.Fatal(err)
	}
	assertRestored(t, orig, n.clone())
	if snap, _ := store.Load(); snap != nil {
		t.Fatalf("store must be cleared, got %+v", snap)
	}
	if m2.Applied() {
		t.Fatal("not applied after recovery")
	}
}

func TestRecoveryNothingStored(t *testing.T) {
	n := newSimNet()
	m, _ := newManager(t, n)
	if err := m.RecoverAtStart(); err != nil {
		t.Fatal(err)
	}
	if len(n.calls) != 0 {
		t.Fatalf("nothing to recover must make no calls: %v", n.calls)
	}
}

func TestRecoveryIgnoresNotAppliedSnapshot(t *testing.T) {
	n := newSimNet()
	m, store := newManager(t, n)
	if err := store.Save(Snapshot{Applied: false, Services: []ServiceSnapshot{{Name: "Wi-Fi"}}}); err != nil {
		t.Fatal(err)
	}
	if err := m.RecoverAtStart(); err != nil {
		t.Fatal(err)
	}
	if len(n.calls) != 0 {
		t.Fatalf("a snapshot not marked applied must not be restored: %v", n.calls)
	}
	if snap, _ := store.Load(); snap != nil {
		t.Fatal("stale snapshot should be cleared")
	}
}

func TestRecoveryCorruptStore(t *testing.T) {
	n := newSimNet()
	m, store := newManager(t, n)
	if err := os.WriteFile(store.Path, []byte("{not json"), 0o600); err != nil {
		t.Fatal(err)
	}
	if err := m.RecoverAtStart(); err != nil {
		t.Fatalf("corrupt store must be ignored, got %v", err)
	}
	if len(n.calls) != 0 {
		t.Fatalf("corrupt store must cause no writes: %v", n.calls)
	}
	if _, err := os.Stat(store.Path); !os.IsNotExist(err) {
		t.Fatal("corrupt file must be moved away")
	}
	if _, err := os.Stat(store.Path + ".corrupt"); err != nil {
		t.Fatalf("expected %s.corrupt: %v", store.Path, err)
	}
}

func TestRecoveryRejectsHostileSnapshot(t *testing.T) {
	n := newSimNet()
	m, store := newManager(t, n)
	// a service name that would be parsed as an option must never reach argv
	data := `{"applied":true,"port":1,"services":[{"name":"-setdnsservers"}]}`
	if err := os.WriteFile(store.Path, []byte(data), 0o600); err != nil {
		t.Fatal(err)
	}
	if err := m.RecoverAtStart(); err != nil {
		t.Fatal(err)
	}
	if len(n.calls) != 0 {
		t.Fatalf("hostile snapshot must cause no calls: %v", n.calls)
	}
	if _, err := os.Stat(store.Path + ".corrupt"); err != nil {
		t.Fatal("hostile snapshot should be quarantined as .corrupt")
	}
}

func TestProxyManagerRejectsBadInput(t *testing.T) {
	cases := []struct {
		name   string
		port   int
		bypass []string
	}{
		{"port 0", 0, nil},
		{"port 65536", 65536, nil},
		{"negative port", -1, nil},
		{"bad token space", 2080, []string{"a b"}},
		{"bad token dash", 2080, []string{"-setwebproxy"}},
		{"bad token empty", 2080, []string{""}},
	}
	for _, c := range cases {
		t.Run(c.name, func(t *testing.T) {
			n := newSimNet()
			m, _ := newManager(t, n)
			if _, _, err := m.Apply(c.port, c.bypass); err == nil {
				t.Fatal("expected error")
			}
			if len(n.calls) != 0 {
				t.Fatalf("no Commander calls allowed, got %v", n.calls)
			}
			if m.Applied() {
				t.Fatal("must not be applied")
			}
		})
	}
}

func TestProxyManagerNoEligibleService(t *testing.T) {
	n := &simNet{svcs: map[string]*simSvc{}, disabled: map[string]bool{}}
	n.add("Happ", "", &simSvc{})
	n.add("Off", "en5", &simSvc{})
	n.disabled["Off"] = true
	m, store := newManager(t, n)
	_, _, err := m.Apply(2080, nil)
	if err == nil || !strings.Contains(err.Error(), "no network service with a hardware device") {
		t.Fatalf("err = %v", err)
	}
	if n.countPrefix("-set") != 0 {
		t.Fatal("no writes expected")
	}
	if snap, _ := store.Load(); snap != nil {
		t.Fatal("nothing must be persisted")
	}
}

func TestProxyManagerSkipsServiceWhoseGettersFail(t *testing.T) {
	n := newSimNet()
	n.fail = func(args []string) *simFailure {
		if args[0] == "-getsecurewebproxy" && args[1] == "USB 10/100/1000 LAN" {
			return &simFailure{out: "** Error: Unable to find item in network database.\n", err: errors.New("exit status 1")}
		}
		return nil
	}
	m, _ := newManager(t, n)
	applied, failed, err := m.Apply(2080, nil)
	if err != nil {
		t.Fatal(err)
	}
	if !reflect.DeepEqual(applied, []string{"Wi-Fi"}) {
		t.Fatalf("applied = %v", applied)
	}
	if len(failed) != 1 || !strings.HasPrefix(failed[0], "USB 10/100/1000 LAN: ") {
		t.Fatalf("failed = %q", failed)
	}
	for _, a := range n.calls {
		if strings.HasPrefix(a[0], "-set") && a[1] == "USB 10/100/1000 LAN" {
			t.Fatalf("a service that could not be snapshotted must not be written: %q", a)
		}
	}
}

func TestProxyManagerReapplyDifferentPortKeepsOriginalSnapshot(t *testing.T) {
	n := newSimNet()
	orig := n.clone()
	m, _ := newManager(t, n)
	if _, _, err := m.Apply(2080, nil); err != nil {
		t.Fatal(err)
	}
	getters := n.countPrefix("-get")
	if _, _, err := m.Apply(3090, nil); err != nil {
		t.Fatal(err)
	}
	if n.countPrefix("-get") != getters {
		t.Fatal("re-apply while applied must not snapshot again (it would capture our own 127.0.0.1 settings)")
	}
	if got := n.svcs["Wi-Fi"].socks; got != (ProxyState{Enabled: true, Server: "127.0.0.1", Port: 3090}) {
		t.Fatalf("new port not applied: %+v", got)
	}
	if err := m.Restore(); err != nil {
		t.Fatal(err)
	}
	assertRestored(t, orig, n.clone())
}

func TestProxyManagerReapplyAfterRestore(t *testing.T) {
	n := newSimNet()
	orig := n.clone()
	m, _ := newManager(t, n)
	states := []bool{}
	states = append(states, m.Applied())
	if _, _, err := m.Apply(2080, nil); err != nil {
		t.Fatal(err)
	}
	states = append(states, m.Applied())
	if err := m.Restore(); err != nil {
		t.Fatal(err)
	}
	states = append(states, m.Applied())
	getters := n.countPrefix("-get")
	if _, _, err := m.Apply(2080, nil); err != nil {
		t.Fatal(err)
	}
	if n.countPrefix("-get") == getters {
		t.Fatal("second Apply after Restore must capture a fresh snapshot")
	}
	states = append(states, m.Applied())
	if err := m.Restore(); err != nil {
		t.Fatal(err)
	}
	states = append(states, m.Applied())
	if !reflect.DeepEqual(states, []bool{false, true, false, true, false}) {
		t.Fatalf("Applied() sequence = %v", states)
	}
	assertRestored(t, orig, n.clone())
}

func TestSnapshotStore(t *testing.T) {
	dir := t.TempDir()
	s := SnapshotStore{Path: filepath.Join(dir, "proxy-snapshot.json")}
	if snap, err := s.Load(); snap != nil || err != nil {
		t.Fatalf("absent store = %+v %v", snap, err)
	}
	in := Snapshot{Applied: true, Port: 2080, Services: []ServiceSnapshot{{
		Name: "Wi-Fi", Web: ProxyState{Enabled: true, Server: "1.2.3.4", Port: 8080},
		Bypass: []string{"*.local"}, AutoURL: "http://x/pac", AutoURLEnabled: true, AutoDiscovery: true,
	}}}
	if err := s.Save(in); err != nil {
		t.Fatal(err)
	}
	out, err := s.Load()
	if err != nil || !reflect.DeepEqual(*out, in) {
		t.Fatalf("round trip = %+v %v", out, err)
	}
	fi, err := os.Stat(s.Path)
	if err != nil {
		t.Fatal(err)
	}
	if fi.Mode().Perm() != 0o600 {
		t.Fatalf("mode = %v, want 0600", fi.Mode().Perm())
	}
	if _, err := os.Stat(s.Path + ".tmp"); !os.IsNotExist(err) {
		t.Fatal("temp file must not be left behind")
	}
	if err := s.Clear(); err != nil {
		t.Fatal(err)
	}
	if err := s.Clear(); err != nil {
		t.Fatalf("clearing an absent store must not fail: %v", err)
	}
	if snap, _ := s.Load(); snap != nil {
		t.Fatal("cleared")
	}
}

func TestSnapshotStoreSaveFailsWhenDirMissing(t *testing.T) {
	s := SnapshotStore{Path: filepath.Join(t.TempDir(), "missing", "proxy-snapshot.json")}
	if err := s.Save(Snapshot{Applied: true}); err == nil {
		t.Fatal("Save must fail (and Apply must then refuse to write) when the support dir is missing")
	}
	n := newSimNet()
	m := &ProxyManager{Cmd: n, Store: s}
	if _, _, err := m.Apply(2080, nil); err == nil {
		t.Fatal("Apply must fail when the snapshot cannot be persisted")
	}
	if n.countPrefix("-set") != 0 {
		t.Fatal("no writes without a persisted snapshot")
	}
}

// ---- gap 1 (uat-round-1): concurrent per-service work, minimal restore ----

func TestApplyRestoreRoundTrip(t *testing.T) {
	proxy := func(en bool) ProxyState { return ProxyState{Enabled: en, Server: "4.4.4.4", Port: 8080} }
	cases := []struct {
		name   string
		svc    simSvc
		bypass []string // Apply argument; nil = default list
	}{
		{"all off", simSvc{}, nil},
		{"pac on", simSvc{autoURL: "http://p/x.pac", autoURLEnabled: true}, nil},
		{"pac configured but off", simSvc{autoURL: "http://p/x.pac"}, nil},
		{"wpad on", simSvc{discovery: true}, nil},
		{"recorded servers disabled", simSvc{web: proxy(false), secure: proxy(false), socks: proxy(false)}, nil},
		{"recorded servers enabled", simSvc{web: proxy(true), secure: proxy(true), socks: proxy(true)}, nil},
		{"custom bypass", simSvc{bypass: []string{"*.corp", "10.1.0.0/16"}}, []string{"127.0.0.1", "localhost"}},
		{"bypass equal to default", simSvc{bypass: DefaultBypass()}, nil},
	}
	for _, c := range cases {
		for _, recover := range []bool{false, true} {
			name := c.name + "/restore"
			if recover {
				name = c.name + "/recover"
			}
			t.Run(name, func(t *testing.T) {
				n := &simNet{svcs: map[string]*simSvc{}, disabled: map[string]bool{}}
				svc := c.svc
				n.add("Only", "en0", &svc)
				orig := n.clone()
				m, store := newManager(t, n)
				if _, _, err := m.Apply(2080, c.bypass); err != nil {
					t.Fatal(err)
				}
				if n.svcs["Only"].web != (ProxyState{Enabled: true, Server: "127.0.0.1", Port: 2080}) {
					t.Fatalf("not applied: %+v", n.svcs["Only"])
				}
				if recover {
					m = &ProxyManager{Cmd: n, Store: store}
					if err := m.RecoverAtStart(); err != nil {
						t.Fatal(err)
					}
				} else if err := m.Restore(); err != nil {
					t.Fatal(err)
				}
				assertRestored(t, orig, n.clone())
			})
		}
	}
}

func TestApplyPersistsAppliedBypassBeforeWrite(t *testing.T) {
	n := newSimNet()
	m, store := newManager(t, n)
	var seen *Snapshot
	n.onSet = func([]string) { seen, _ = store.Load() }
	if _, _, err := m.Apply(2080, []string{"127.0.0.1", "a.example"}); err != nil {
		t.Fatal(err)
	}
	if seen == nil || !reflect.DeepEqual(seen.AppliedBypass, []string{"127.0.0.1", "a.example"}) {
		t.Fatalf("applied bypass not persisted before the first write: %+v", seen)
	}
	// re-apply while applied with another list: the disk copy must hold it before the first write
	seen = nil
	n.setSeen = false
	if _, _, err := m.Apply(2080, []string{"127.0.0.1", "b.example"}); err != nil {
		t.Fatal(err)
	}
	if seen == nil || !reflect.DeepEqual(seen.AppliedBypass, []string{"127.0.0.1", "b.example"}) {
		t.Fatalf("re-apply must update the applied bypass before writing: %+v", seen)
	}
}

func TestRestoreSkipsNoopWrites(t *testing.T) {
	n := newSimNet4()
	m, _ := newManager(t, n)
	if _, _, err := m.Apply(2080, nil); err != nil {
		t.Fatal(err)
	}
	n.resetLog()
	if err := m.Restore(); err != nil {
		t.Fatal(err)
	}
	if got := len(n.writes["USB 10/100/1000 LAN"]); got > 4 {
		t.Fatalf("all-off service restored with %d writes: %q", got, n.writes["USB 10/100/1000 LAN"])
	}
	for _, w := range n.writes["USB 10/100/1000 LAN"] {
		switch w[0] {
		case "-setautoproxyurl", "-setautoproxystate", "-setproxyautodiscovery":
			t.Fatalf("untouched PAC/WPAD written back: %q", w)
		}
	}
}

func TestApplyConcurrentAcrossServices(t *testing.T) {
	const delay = 20 * time.Millisecond
	n := newSimNet4()
	n.delay = delay
	m, _ := newManager(t, n)
	start := time.Now()
	applied, failed, err := m.Apply(2080, []string{"127.0.0.1", "localhost"})
	elapsed := time.Since(start)
	if err != nil || len(failed) != 0 {
		t.Fatalf("applied=%v failed=%v err=%v", applied, failed, err)
	}
	want := []string{"Wi-Fi", "USB 10/100/1000 LAN", "Thunderbolt Bridge", "iPhone USB"}
	if !reflect.DeepEqual(applied, want) {
		t.Fatalf("applied = %v, want service order %v", applied, want)
	}
	seq := time.Duration(len(n.calls)) * delay
	if elapsed >= seq*6/10 {
		t.Fatalf("Apply took %v for %d calls (sequential would be %v): not concurrent", elapsed, len(n.calls), seq)
	}
	if n.maxInflight < 2 {
		t.Fatalf("max concurrent Run = %d, want > 1", n.maxInflight)
	}
	for _, name := range want {
		s := n.svcs[name]
		p := ProxyState{Enabled: true, Server: "127.0.0.1", Port: 2080}
		if s.web != p || s.secure != p || s.socks != p || !reflect.DeepEqual(s.bypass, []string{"127.0.0.1", "localhost"}) || s.autoURLEnabled || s.discovery {
			t.Errorf("%s final state = %+v", name, *s)
		}
	}
	if len(n.overlap) != 0 {
		t.Fatalf("writes of one service overlapped: %v", n.overlap)
	}
}

func TestPerServiceOrderPreserved(t *testing.T) {
	n := newSimNet4()
	n.delay = 3 * time.Millisecond
	m, store := newManager(t, n)
	bypass := []string{"127.0.0.1", "localhost"}
	n.resetLog()
	if _, _, err := m.Apply(2080, bypass); err != nil {
		t.Fatal(err)
	}
	snap, err := store.Load()
	if err != nil || snap == nil {
		t.Fatalf("load: %v %v", snap, err)
	}
	for _, svc := range snap.Services {
		want := ApplyPlan(Snapshot{Services: []ServiceSnapshot{svc}}, 2080, bypass)
		if got := n.writes[svc.Name]; !reflect.DeepEqual(got, want) {
			t.Errorf("%s apply order\n got: %q\nwant: %q", svc.Name, got, want)
		}
	}
	if len(n.overlap) != 0 {
		t.Fatalf("writes of one service overlapped in time: %v", n.overlap)
	}
	n.resetLog()
	if err := m.Restore(); err != nil {
		t.Fatal(err)
	}
	for _, svc := range snap.Services {
		want := RestorePlan(Snapshot{Services: []ServiceSnapshot{svc}}, snap.AppliedBypass)
		if got := n.writes[svc.Name]; !reflect.DeepEqual(got, want) {
			t.Errorf("%s restore order\n got: %q\nwant: %q", svc.Name, got, want)
		}
	}
	if len(n.overlap) != 0 {
		t.Fatalf("restore writes of one service overlapped in time: %v", n.overlap)
	}
}

func TestRestoreConcurrentAcrossServices(t *testing.T) {
	const delay = 20 * time.Millisecond
	for _, recover := range []bool{false, true} {
		name := "restore"
		if recover {
			name = "recover"
		}
		t.Run(name, func(t *testing.T) {
			n := newSimNet4()
			orig := n.clone()
			m, store := newManager(t, n)
			if _, _, err := m.Apply(2080, nil); err != nil {
				t.Fatal(err)
			}
			n.resetLog()
			n.delay = delay
			start := time.Now()
			var err error
			if recover {
				err = (&ProxyManager{Cmd: n, Store: store}).RecoverAtStart()
			} else {
				err = m.Restore()
			}
			elapsed := time.Since(start)
			if err != nil {
				t.Fatal(err)
			}
			seq := time.Duration(len(n.calls)) * delay
			if elapsed >= seq*6/10 {
				t.Fatalf("took %v for %d calls (sequential would be %v): not concurrent", elapsed, len(n.calls), seq)
			}
			if n.maxInflight < 2 {
				t.Fatalf("max concurrent Run = %d, want > 1", n.maxInflight)
			}
			if len(n.overlap) != 0 {
				t.Fatalf("writes of one service overlapped: %v", n.overlap)
			}
			assertRestored(t, orig, n.clone())
		})
	}
}

func TestConcurrentWriteFailureRetriedSequentially(t *testing.T) {
	n := newSimNet4()
	n.failFirstWrite("Thunderbolt Bridge", simFailure{out: "** Error: preferences locked\n", err: errors.New("exit status 1")})
	m, _ := newManager(t, n)
	applied, failed, err := m.Apply(2080, nil)
	if err != nil {
		t.Fatal(err)
	}
	if len(failed) != 0 || !reflect.DeepEqual(applied, []string{"Wi-Fi", "USB 10/100/1000 LAN", "Thunderbolt Bridge", "iPhone USB"}) {
		t.Fatalf("a transient failure must be retried: applied=%v failed=%v", applied, failed)
	}
	if got := n.svcs["Thunderbolt Bridge"].web; got != (ProxyState{Enabled: true, Server: "127.0.0.1", Port: 2080}) {
		t.Fatalf("retried service not applied: %+v", got)
	}

	// failing twice: reported, with its error, and the others stay applied
	n2 := newSimNet4()
	n2.fail = func(args []string) *simFailure {
		if len(args) > 1 && args[0] == "-setsecurewebproxy" && args[1] == "iPhone USB" {
			return &simFailure{out: "** Error: preferences locked\n", err: errors.New("exit status 1")}
		}
		return nil
	}
	m2, _ := newManager(t, n2)
	applied, failed, err = m2.Apply(2080, nil)
	if err != nil {
		t.Fatal(err)
	}
	if len(applied) != 3 || len(failed) != 1 || !strings.HasPrefix(failed[0], "iPhone USB: ") || !strings.Contains(failed[0], "preferences locked") {
		t.Fatalf("applied=%v failed=%q", applied, failed)
	}
}

func TestRestoreFailureRetriedSequentially(t *testing.T) {
	n := newSimNet4()
	orig := n.clone()
	m, _ := newManager(t, n)
	if _, _, err := m.Apply(2080, nil); err != nil {
		t.Fatal(err)
	}
	n.failFirstWrite("iPhone USB", simFailure{out: "** Error: preferences locked\n", err: errors.New("exit status 1")})
	if err := m.Restore(); err != nil {
		t.Fatalf("a transient restore failure must be retried: %v", err)
	}
	assertRestored(t, orig, n.clone())
	if m.Applied() {
		t.Fatal("fully restored")
	}
}

func TestPartialApplyForgetsAppliedBypass(t *testing.T) {
	n := newSimNet()
	orig := n.clone()
	n.fail = func(args []string) *simFailure {
		if len(args) > 1 && args[0] == "-setsocksfirewallproxy" && args[1] == "Wi-Fi" {
			return &simFailure{out: "** Error: nope\n", err: errors.New("exit status 1")}
		}
		return nil
	}
	m, store := newManager(t, n)
	if _, failed, err := m.Apply(2080, nil); err != nil || len(failed) != 1 {
		t.Fatalf("failed=%v err=%v", failed, err)
	}
	snap, _ := store.Load()
	if snap == nil || snap.AppliedBypass != nil {
		t.Fatalf("a partly failed apply must leave the applied bypass unknown: %+v", snap)
	}
	n.fail = nil
	if err := m.Restore(); err != nil {
		t.Fatal(err)
	}
	assertRestored(t, orig, n.clone())
}

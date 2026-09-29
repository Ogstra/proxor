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
	n.mu.Lock()
	defer n.mu.Unlock()
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

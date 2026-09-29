package machelper

import (
	"encoding/json"
	"errors"
	"fmt"
	"log"
	"os"
	"path/filepath"
	"strings"
	"sync"
)

// Commander runs /usr/sbin/networksetup with the given argv (no shell) and
// returns its stdout. The real executor lives in the darwin-only helper
// code; tests use a fake so Linux CI proves the logic.
type Commander interface {
	Run(args ...string) (string, error)
}

// SnapshotStore persists the pre-Apply proxy state so a helper crash or a
// reboot cannot leave the machine pointed at a dead 127.0.0.1 proxy.
// The real path is SupportDir + "/proxy-snapshot.json".
type SnapshotStore struct{ Path string }

// Load returns nil, nil when no snapshot exists.
func (s SnapshotStore) Load() (*Snapshot, error) {
	data, err := os.ReadFile(s.Path)
	if errors.Is(err, os.ErrNotExist) {
		return nil, nil
	}
	if err != nil {
		return nil, err
	}
	var snap Snapshot
	if err := json.Unmarshal(data, &snap); err != nil {
		return nil, fmt.Errorf("snapshot %s: %w", s.Path, err)
	}
	for _, svc := range snap.Services {
		if !safeServiceName(svc.Name) {
			return nil, fmt.Errorf("snapshot %s: unsafe service name %q", s.Path, svc.Name)
		}
	}
	return &snap, nil
}

// Save writes <Path>.tmp (0600), fsyncs it and renames it over Path, so a
// crash leaves either the old or the new snapshot, never a torn one. The
// directory must already exist (the installer creates it).
func (s SnapshotStore) Save(snap Snapshot) error {
	data, err := json.Marshal(snap)
	if err != nil {
		return err
	}
	tmp := s.Path + ".tmp"
	f, err := os.OpenFile(tmp, os.O_WRONLY|os.O_CREATE|os.O_TRUNC, 0o600)
	if err != nil {
		return err
	}
	if _, err := f.Write(data); err != nil {
		f.Close()
		os.Remove(tmp)
		return err
	}
	if err := f.Sync(); err != nil {
		f.Close()
		os.Remove(tmp)
		return err
	}
	if err := f.Close(); err != nil {
		os.Remove(tmp)
		return err
	}
	if err := os.Rename(tmp, s.Path); err != nil {
		os.Remove(tmp)
		return err
	}
	if d, err := os.Open(filepath.Dir(s.Path)); err == nil { // make the rename durable, best effort
		d.Sync()
		d.Close()
	}
	return nil
}

func (s SnapshotStore) Clear() error {
	if err := os.Remove(s.Path); err != nil && !errors.Is(err, os.ErrNotExist) {
		return err
	}
	return nil
}

// ProxyManager applies and restores the macOS system proxy through networksetup.
// Its method set (Apply/Restore/Applied) matches ProxyBackend.
type ProxyManager struct {
	Cmd   Commander
	Store SnapshotStore

	mu      sync.Mutex
	current *Snapshot // non-nil while our settings may be live
}

func safeServiceName(name string) bool {
	return name != "" && !strings.HasPrefix(name, "-")
}

// run executes networksetup. "** Error:" output is a failure even when the
// exit status is 0 (networksetup does that for several errors).
func (m *ProxyManager) run(args ...string) (string, error) {
	out, err := m.Cmd.Run(args...)
	if err == nil {
		err = checkNoError(out)
	} else if msg := strings.TrimSpace(out); msg != "" {
		err = fmt.Errorf("%w: %s", err, msg)
	}
	return out, err
}

func (m *ProxyManager) Applied() bool {
	m.mu.Lock()
	defer m.mu.Unlock()
	return m.current != nil && m.current.Applied
}

// Apply points every eligible service at 127.0.0.1:port. The snapshot of the
// original settings is persisted before the first write. Per-service errors
// are reported in failed; err is set only when nothing could be applied.
// Calling Apply again while applied re-applies with the new port and keeps
// the ORIGINAL snapshot (it never records its own 127.0.0.1 settings).
func (m *ProxyManager) Apply(port int, bypass []string) (applied []string, failed []string, err error) {
	if port < 1 || port > 65535 {
		return nil, nil, fmt.Errorf("invalid proxy port %d", port)
	}
	if len(bypass) == 0 {
		bypass = DefaultBypass()
	}
	if err := ValidateBypass(bypass); err != nil {
		return nil, nil, err
	}

	m.mu.Lock()
	defer m.mu.Unlock()

	wasApplied := m.current != nil && m.current.Applied
	var snap Snapshot
	if wasApplied {
		snap = *m.current
	} else {
		snap, failed, err = m.capture()
		if err != nil {
			return nil, failed, err
		}
	}
	snap.Applied = true
	snap.Port = port
	if err := m.Store.Save(snap); err != nil {
		return nil, failed, fmt.Errorf("persisting proxy snapshot: %w", err)
	}
	m.current = &snap

	for _, svc := range snap.Services {
		plan := ApplyPlan(Snapshot{Services: []ServiceSnapshot{svc}}, port, bypass)
		if perr := m.runPlan(plan); perr != nil {
			failed = append(failed, fmt.Sprintf("%s: %v", svc.Name, perr))
			continue
		}
		applied = append(applied, svc.Name)
	}

	if len(applied) == 0 {
		err = errors.New("system proxy could not be applied to any network service")
		if !wasApplied {
			// Roll back whatever the failed attempts changed, then forget the snapshot.
			m.restoreServices(snap.Services)
			m.current = nil
			if cerr := m.Store.Clear(); cerr != nil {
				log.Printf("machelper: clearing proxy snapshot after rollback: %v", cerr)
			}
		}
		return nil, failed, err
	}
	return applied, failed, nil
}

// runPlan runs every argv, stopping at the first failure.
func (m *ProxyManager) runPlan(plan [][]string) error {
	for _, argv := range plan {
		if _, err := m.run(argv...); err != nil {
			return fmt.Errorf("%s: %w", argv[0], err)
		}
	}
	return nil
}

// capture snapshots every eligible service with the six read-only getters.
// A service whose getters fail is skipped and reported in failed.
func (m *ProxyManager) capture() (Snapshot, []string, error) {
	orderOut, err := m.run("-listnetworkserviceorder")
	if err != nil {
		return Snapshot{}, nil, fmt.Errorf("listing network services: %w", err)
	}
	order, err := ParseServiceOrder(orderOut)
	if err != nil {
		return Snapshot{}, nil, err
	}
	allOut, err := m.run("-listallnetworkservices")
	if err != nil {
		return Snapshot{}, nil, fmt.Errorf("listing network services: %w", err)
	}
	all, err := ParseAllServices(allOut)
	if err != nil {
		return Snapshot{}, nil, err
	}
	eligible := EligibleServices(order, all)
	if len(eligible) == 0 {
		return Snapshot{}, nil, errors.New("no network service with a hardware device")
	}

	var snap Snapshot
	var failed []string
	for _, svc := range eligible {
		if !safeServiceName(svc.Name) {
			failed = append(failed, fmt.Sprintf("%s: unsafe service name", svc.Name))
			continue
		}
		ss, err := m.snapshotService(svc.Name)
		if err != nil {
			failed = append(failed, fmt.Sprintf("%s: %v", svc.Name, err))
			continue
		}
		snap.Services = append(snap.Services, ss)
	}
	if len(snap.Services) == 0 {
		return Snapshot{}, failed, errors.New("no network service could be read")
	}
	return snap, failed, nil
}

func (m *ProxyManager) snapshotService(name string) (ServiceSnapshot, error) {
	ss := ServiceSnapshot{Name: name}
	proxies := []struct {
		cmd string
		dst *ProxyState
	}{
		{"-getwebproxy", &ss.Web},
		{"-getsecurewebproxy", &ss.Secure},
		{"-getsocksfirewallproxy", &ss.Socks},
	}
	for _, p := range proxies {
		out, err := m.run(p.cmd, name)
		if err != nil {
			return ss, fmt.Errorf("%s: %w", p.cmd, err)
		}
		if *p.dst, err = ParseProxyState(out); err != nil {
			return ss, fmt.Errorf("%s: %w", p.cmd, err)
		}
	}
	out, err := m.run("-getproxybypassdomains", name)
	if err != nil {
		return ss, fmt.Errorf("-getproxybypassdomains: %w", err)
	}
	if ss.Bypass, err = ParseBypassDomains(out); err != nil {
		return ss, fmt.Errorf("-getproxybypassdomains: %w", err)
	}
	out, err = m.run("-getautoproxyurl", name)
	if err != nil {
		return ss, fmt.Errorf("-getautoproxyurl: %w", err)
	}
	auto, err := ParseAutoProxyURL(out)
	if err != nil {
		return ss, fmt.Errorf("-getautoproxyurl: %w", err)
	}
	ss.AutoURL, ss.AutoURLEnabled = auto.URL, auto.Enabled
	out, err = m.run("-getproxyautodiscovery", name)
	if err != nil {
		return ss, fmt.Errorf("-getproxyautodiscovery: %w", err)
	}
	if ss.AutoDiscovery, err = ParseAutoDiscovery(out); err != nil {
		return ss, fmt.Errorf("-getproxyautodiscovery: %w", err)
	}
	return ss, nil
}

// restoreServices writes back every service's recorded state, best effort:
// all commands of a service run even after one fails. It returns the
// services that did not restore cleanly and the errors.
func (m *ProxyManager) restoreServices(services []ServiceSnapshot) (remaining []ServiceSnapshot, errs []error) {
	for _, svc := range services {
		var svcErrs []error
		for _, argv := range RestorePlan(Snapshot{Services: []ServiceSnapshot{svc}}) {
			if _, err := m.run(argv...); err != nil {
				svcErrs = append(svcErrs, fmt.Errorf("%s: %s: %w", svc.Name, argv[0], err))
			}
		}
		if len(svcErrs) > 0 {
			remaining = append(remaining, svc)
			errs = append(errs, svcErrs...)
		}
	}
	return remaining, errs
}

// finishRestore records the outcome of a restore attempt: fully restored
// clears the store; otherwise only the services that failed stay recorded (as
// applied) so a later Restore or RecoverAtStart retries them.
func (m *ProxyManager) finishRestore(snap Snapshot, remaining []ServiceSnapshot, errs []error) error {
	if len(remaining) == 0 {
		m.current = nil
		if err := m.Store.Clear(); err != nil {
			return fmt.Errorf("clearing proxy snapshot: %w", err)
		}
		return nil
	}
	snap.Services = remaining
	snap.Applied = true
	m.current = &snap
	if err := m.Store.Save(snap); err != nil {
		errs = append(errs, fmt.Errorf("persisting remaining proxy snapshot: %w", err))
	}
	return fmt.Errorf("system proxy restore incomplete: %w", errors.Join(errs...))
}

// Restore writes back the recorded settings. Nothing applied is a no-op.
func (m *ProxyManager) Restore() error {
	m.mu.Lock()
	defer m.mu.Unlock()
	if m.current == nil {
		return nil
	}
	snap := *m.current
	remaining, errs := m.restoreServices(snap.Services)
	return m.finishRestore(snap, remaining, errs)
}

// RecoverAtStart runs when the helper starts: a persisted snapshot marked
// applied means the previous helper (or the whole machine) went away while
// our proxy settings were live, so they are restored now. An unreadable
// snapshot is renamed to <name>.corrupt and ignored.
func (m *ProxyManager) RecoverAtStart() error {
	m.mu.Lock()
	defer m.mu.Unlock()
	snap, err := m.Store.Load()
	if err != nil {
		log.Printf("machelper: ignoring unusable proxy snapshot: %v", err)
		if rerr := os.Rename(m.Store.Path, m.Store.Path+".corrupt"); rerr != nil && !errors.Is(rerr, os.ErrNotExist) {
			return fmt.Errorf("quarantining corrupt proxy snapshot: %w", rerr)
		}
		return nil
	}
	if snap == nil {
		return nil
	}
	if !snap.Applied {
		if err := m.Store.Clear(); err != nil {
			return fmt.Errorf("clearing stale proxy snapshot: %w", err)
		}
		return nil
	}
	remaining, errs := m.restoreServices(snap.Services)
	return m.finishRestore(*snap, remaining, errs)
}

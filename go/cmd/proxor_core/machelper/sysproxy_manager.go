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
	// Recorded before the first write (also on a re-apply) so a restore or a
	// crash recovery compares against the list really on the services.
	snap.AppliedBypass = append([]string(nil), bypass...)
	if err := m.Store.Save(snap); err != nil {
		return nil, failed, fmt.Errorf("persisting proxy snapshot: %w", err)
	}
	m.current = &snap

	errs := m.applyServices(snap.Services, port, bypass)
	var applyFailed bool
	for i, svc := range snap.Services {
		if errs[i] != nil {
			applyFailed = true
			failed = append(failed, fmt.Sprintf("%s: %v", svc.Name, errs[i]))
			continue
		}
		applied = append(applied, svc.Name)
	}

	if len(applied) == 0 {
		err = errors.New("system proxy could not be applied to any network service")
		if !wasApplied {
			// Roll back whatever the failed attempts changed, then forget the snapshot.
			m.restoreServices(snap.Services, nil)
			m.current = nil
			if cerr := m.Store.Clear(); cerr != nil {
				log.Printf("machelper: clearing proxy snapshot after rollback: %v", cerr)
			}
		}
		return nil, failed, err
	}
	if applyFailed {
		// A service that failed part-way may hold a different bypass list than
		// the one recorded: forget it so restore writes the recorded list back
		// unconditionally.
		snap.AppliedBypass = nil
		m.current = &snap
		if serr := m.Store.Save(snap); serr != nil {
			log.Printf("machelper: updating proxy snapshot after partial apply: %v", serr)
		}
	}
	return applied, failed, nil
}

// maxParallelServices bounds how many services are processed at once.
const maxParallelServices = 8

// forEachService runs fn(0..n-1), at most limit at a time, and waits for all.
func forEachService(n, limit int, fn func(i int)) {
	if limit < 1 {
		limit = 1
	}
	sem := make(chan struct{}, limit)
	var wg sync.WaitGroup
	for i := 0; i < n; i++ {
		wg.Add(1)
		sem <- struct{}{}
		go func(i int) {
			defer wg.Done()
			defer func() { <-sem }()
			fn(i)
		}(i)
	}
	wg.Wait()
}

// applyServices applies every service concurrently (the commands of one
// service stay in order), then retries each failed service once, sequentially,
// in service order. The result holds one error (or nil) per service.
func (m *ProxyManager) applyServices(services []ServiceSnapshot, port int, bypass []string) []error {
	one := func(svc ServiceSnapshot) error {
		return m.runPlan(ApplyPlan(Snapshot{Services: []ServiceSnapshot{svc}}, port, bypass))
	}
	errs := make([]error, len(services))
	forEachService(len(services), maxParallelServices, func(i int) {
		errs[i] = one(services[i])
	})
	for i, svc := range services {
		if errs[i] != nil {
			errs[i] = one(svc)
		}
	}
	return errs
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

	type result struct {
		ss  ServiceSnapshot
		err error
	}
	results := make([]result, len(eligible))
	forEachService(len(eligible), maxParallelServices, func(i int) {
		name := eligible[i].Name
		if !safeServiceName(name) {
			results[i].err = errors.New("unsafe service name")
			return
		}
		results[i].ss, results[i].err = m.snapshotService(name)
	})
	var snap Snapshot
	var failed []string
	for i, r := range results {
		if r.err != nil {
			failed = append(failed, fmt.Sprintf("%s: %v", eligible[i].Name, r.err))
			continue
		}
		snap.Services = append(snap.Services, r.ss)
	}
	if len(snap.Services) == 0 {
		return Snapshot{}, failed, errors.New("no network service could be read")
	}
	return snap, failed, nil
}

// snapshotService reads the six getters of one service concurrently (they are
// read-only and independent). The first failing getter, in the fixed getter
// order, is the reported error.
func (m *ProxyManager) snapshotService(name string) (ServiceSnapshot, error) {
	ss := ServiceSnapshot{Name: name}
	getters := []struct {
		cmd   string
		parse func(out string) error
	}{
		{"-getwebproxy", func(out string) (err error) { ss.Web, err = ParseProxyState(out); return }},
		{"-getsecurewebproxy", func(out string) (err error) { ss.Secure, err = ParseProxyState(out); return }},
		{"-getsocksfirewallproxy", func(out string) (err error) { ss.Socks, err = ParseProxyState(out); return }},
		{"-getproxybypassdomains", func(out string) (err error) { ss.Bypass, err = ParseBypassDomains(out); return }},
		{"-getautoproxyurl", func(out string) error {
			auto, err := ParseAutoProxyURL(out)
			ss.AutoURL, ss.AutoURLEnabled = auto.URL, auto.Enabled
			return err
		}},
		{"-getproxyautodiscovery", func(out string) (err error) { ss.AutoDiscovery, err = ParseAutoDiscovery(out); return }},
	}
	errs := make([]error, len(getters))
	var wg sync.WaitGroup
	for i := range getters {
		wg.Add(1)
		go func(i int) { // each getter writes only its own ss fields
			defer wg.Done()
			out, err := m.run(getters[i].cmd, name)
			if err == nil {
				err = getters[i].parse(out)
			}
			errs[i] = err
		}(i)
	}
	wg.Wait()
	for i, err := range errs {
		if err != nil {
			return ss, fmt.Errorf("%s: %w", getters[i].cmd, err)
		}
	}
	return ss, nil
}

// restoreServices writes back every service's recorded state, best effort:
// all commands of a service run even after one fails. Services run
// concurrently (one service's commands stay in order); a service with any
// error is retried once, sequentially, in service order. It returns the
// services that did not restore cleanly and the errors, in service order.
func (m *ProxyManager) restoreServices(services []ServiceSnapshot, appliedBypass []string) (remaining []ServiceSnapshot, errs []error) {
	one := func(svc ServiceSnapshot) (svcErrs []error) {
		for _, argv := range RestorePlan(Snapshot{Services: []ServiceSnapshot{svc}}, appliedBypass) {
			if _, err := m.run(argv...); err != nil {
				svcErrs = append(svcErrs, fmt.Errorf("%s: %s: %w", svc.Name, argv[0], err))
			}
		}
		return svcErrs
	}
	results := make([][]error, len(services))
	forEachService(len(services), maxParallelServices, func(i int) {
		results[i] = one(services[i])
	})
	for i, svc := range services {
		if len(results[i]) > 0 {
			results[i] = one(svc)
		}
		if len(results[i]) > 0 {
			remaining = append(remaining, svc)
			errs = append(errs, results[i]...)
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
	remaining, errs := m.restoreServices(snap.Services, snap.AppliedBypass)
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
	remaining, errs := m.restoreServices(snap.Services, snap.AppliedBypass)
	return m.finishRestore(*snap, remaining, errs)
}

package machelper

import (
	"errors"
	"fmt"
	"strconv"
	"strings"
)

// This file is pure: it parses the text printed by /usr/sbin/networksetup and
// builds argv lists for it. Running them is sysproxy_manager.go's Commander.
// No build tags: Linux CI runs it against captured real macOS output.

// NetworkService is one entry of `networksetup -listnetworkserviceorder`.
// Network-Extension VPN services (WireGuard, Happ, Tailscale, ...) show an
// empty hardware Device and must not be touched.
type NetworkService struct {
	Name     string
	Device   string
	Disabled bool
}

type ProxyState struct {
	Enabled bool   `json:"enabled"`
	Server  string `json:"server"`
	Port    int    `json:"port"`
}

// AutoProxy is the PAC (`-getautoproxyurl`) state.
type AutoProxy struct {
	URL     string
	Enabled bool
}

type ServiceSnapshot struct {
	Name           string     `json:"name"`
	Web            ProxyState `json:"web"`
	Secure         ProxyState `json:"secure"`
	Socks          ProxyState `json:"socks"`
	Bypass         []string   `json:"bypass"`
	AutoURL        string     `json:"autoUrl"`
	AutoURLEnabled bool       `json:"autoUrlEnabled"`
	AutoDiscovery  bool       `json:"autoDiscovery"`
}

// Snapshot is what the helper persists before its first write. Applied means
// the proxy settings may currently be ours and must be restored on recovery.
type Snapshot struct {
	Applied  bool              `json:"applied"`
	Port     int               `json:"port"`
	Services []ServiceSnapshot `json:"services"`
	// AppliedBypass is the bypass list Apply wrote to the services. Restore
	// skips the bypass write when the recorded list is identical. nil (an
	// old snapshot, or an apply that partly failed) means "unknown": the
	// recorded list is always written back.
	AppliedBypass []string `json:"appliedBypass,omitempty"`
}

// DefaultBypass is applied when the client sends no bypass list.
func DefaultBypass() []string {
	return []string{
		"127.0.0.1", "localhost", "*.local",
		"169.254.0.0/16", "10.0.0.0/8", "172.16.0.0/12", "192.168.0.0/16", "100.64.0.0/10",
	}
}

func checkNoError(out string) error {
	for _, line := range strings.Split(out, "\n") {
		if strings.HasPrefix(strings.TrimSpace(line), "** Error:") {
			return errors.New(strings.TrimSpace(line))
		}
	}
	return nil
}

// ParseServiceOrder parses `networksetup -listnetworkserviceorder`:
//
//	(1) Wi-Fi
//	(Hardware Port: Wi-Fi, Device: en0)
//
// A disabled service is listed as `(*) Name`.
func ParseServiceOrder(out string) ([]NetworkService, error) {
	if err := checkNoError(out); err != nil {
		return nil, err
	}
	var services []NetworkService
	for _, raw := range strings.Split(out, "\n") {
		line := strings.TrimRight(raw, " \t\r")
		if line == "" {
			continue
		}
		if name, disabled, ok := parseServiceHeader(line); ok {
			services = append(services, NetworkService{Name: name, Disabled: disabled})
			continue
		}
		if strings.HasPrefix(line, "(Hardware Port:") && strings.HasSuffix(line, ")") && len(services) > 0 {
			inner := strings.TrimSuffix(line, ")")
			// The hardware port may itself contain ", " but never ", Device: ".
			if i := strings.LastIndex(inner, ", Device:"); i >= 0 {
				services[len(services)-1].Device = strings.TrimSpace(inner[i+len(", Device:"):])
			}
		}
	}
	return services, nil
}

// parseServiceHeader recognises "(N) Name" and "(*) Name".
func parseServiceHeader(line string) (name string, disabled bool, ok bool) {
	if !strings.HasPrefix(line, "(") {
		return "", false, false
	}
	end := strings.Index(line, ") ")
	if end < 0 {
		return "", false, false
	}
	marker := line[1:end]
	if marker == "*" {
		disabled = true
	} else if _, err := strconv.Atoi(marker); err != nil {
		return "", false, false
	}
	name = line[end+2:]
	if name == "" {
		return "", false, false
	}
	return name, disabled, true
}

// ParseAllServices parses `networksetup -listallnetworkservices`: a header
// line, then one name per line, `*`-prefixed when disabled.
func ParseAllServices(out string) ([]NetworkService, error) {
	if err := checkNoError(out); err != nil {
		return nil, err
	}
	var services []NetworkService
	for _, raw := range strings.Split(out, "\n") {
		line := strings.TrimRight(raw, " \t\r")
		if line == "" || strings.HasPrefix(line, "An asterisk (*) denotes") {
			continue
		}
		if strings.HasPrefix(line, "*") {
			services = append(services, NetworkService{Name: strings.TrimPrefix(line, "*"), Disabled: true})
			continue
		}
		services = append(services, NetworkService{Name: line})
	}
	return services, nil
}

// EligibleServices keeps, in `order`'s order, the services that are enabled
// (per either listing) and have a non-empty hardware Device.
func EligibleServices(order, all []NetworkService) []NetworkService {
	disabled := map[string]bool{}
	for _, s := range all {
		if s.Disabled {
			disabled[s.Name] = true
		}
	}
	var out []NetworkService
	for _, s := range order {
		if s.Disabled || disabled[s.Name] || s.Device == "" {
			continue
		}
		out = append(out, s)
	}
	return out
}

// ParseHasIPv4 parses `networksetup -getinfo <service>` and reports whether
// the service currently has an IPv4 address ("IP address: x.x.x.x"). A missing,
// empty or "none" value means no address. Only the first "IP address:" line
// counts; "IPv6 IP address:" and "Router:" lines are different keys.
func ParseHasIPv4(out string) (bool, error) {
	if err := checkNoError(out); err != nil {
		return false, err
	}
	for _, line := range strings.Split(out, "\n") {
		k, v, ok := strings.Cut(line, ":")
		if !ok || strings.TrimSpace(k) != "IP address" {
			continue
		}
		v = strings.TrimSpace(v)
		return v != "" && !strings.EqualFold(v, "none"), nil
	}
	return false, nil
}

// keyValues splits "Key: value" lines (value may be empty).
func keyValues(out string) map[string]string {
	kv := map[string]string{}
	for _, line := range strings.Split(out, "\n") {
		k, v, ok := strings.Cut(line, ":")
		if !ok {
			continue
		}
		kv[strings.TrimSpace(k)] = strings.TrimSpace(v)
	}
	return kv
}

func parseYesNo(v string) (bool, error) {
	switch strings.ToLower(v) {
	case "yes", "on", "1":
		return true, nil
	case "no", "off", "0":
		return false, nil
	}
	return false, fmt.Errorf("unexpected boolean %q", v)
}

// ParseProxyState parses -getwebproxy / -getsecurewebproxy / -getsocksfirewallproxy.
func ParseProxyState(out string) (ProxyState, error) {
	if err := checkNoError(out); err != nil {
		return ProxyState{}, err
	}
	kv := keyValues(out)
	enabledRaw, ok := kv["Enabled"]
	if !ok {
		return ProxyState{}, errors.New("proxy state: missing Enabled")
	}
	portRaw, ok := kv["Port"]
	if !ok {
		return ProxyState{}, errors.New("proxy state: missing Port")
	}
	enabled, err := parseYesNo(enabledRaw)
	if err != nil {
		return ProxyState{}, fmt.Errorf("proxy state: %w", err)
	}
	port, err := strconv.Atoi(portRaw)
	if err != nil || port < 0 || port > 65535 {
		return ProxyState{}, fmt.Errorf("proxy state: bad Port %q", portRaw)
	}
	return ProxyState{Enabled: enabled, Server: kv["Server"], Port: port}, nil
}

// ParseBypassDomains parses -getproxybypassdomains: one entry per line, or a
// sentence when the list is empty.
func ParseBypassDomains(out string) ([]string, error) {
	if err := checkNoError(out); err != nil {
		return nil, err
	}
	if strings.Contains(out, "There aren't any bypass domains set") {
		return []string{}, nil
	}
	list := []string{}
	for _, line := range strings.Split(out, "\n") {
		if t := strings.TrimSpace(line); t != "" {
			list = append(list, t)
		}
	}
	return list, nil
}

// ParseAutoProxyURL parses -getautoproxyurl ("URL: (null)" means none).
func ParseAutoProxyURL(out string) (AutoProxy, error) {
	if err := checkNoError(out); err != nil {
		return AutoProxy{}, err
	}
	kv := keyValues(out)
	enabledRaw, ok := kv["Enabled"]
	if !ok {
		return AutoProxy{}, errors.New("auto proxy url: missing Enabled")
	}
	enabled, err := parseYesNo(enabledRaw)
	if err != nil {
		return AutoProxy{}, fmt.Errorf("auto proxy url: %w", err)
	}
	url := kv["URL"] // keyValues cuts on the first colon only, so the URL keeps its own
	if url == "(null)" {
		url = ""
	}
	return AutoProxy{URL: url, Enabled: enabled}, nil
}

// ParseAutoDiscovery parses -getproxyautodiscovery ("Auto Proxy Discovery: On").
func ParseAutoDiscovery(out string) (bool, error) {
	if err := checkNoError(out); err != nil {
		return false, err
	}
	v, ok := keyValues(out)["Auto Proxy Discovery"]
	if !ok {
		return false, errors.New("auto discovery: missing 'Auto Proxy Discovery'")
	}
	return parseYesNo(v)
}

func onOff(b bool) string {
	if b {
		return "on"
	}
	return "off"
}

// ApplyPlan returns the networksetup argv lists (without the binary) that
// point every snapshotted service at 127.0.0.1:port with the bypass list.
// -setwebproxy/-setsecurewebproxy/-setsocksfirewallproxy already turn the
// proxy on, so no separate `-set*proxystate on` write is emitted.
// PAC/WPAD are turned off only for services where they were on.
func ApplyPlan(snap Snapshot, port int, bypass []string) [][]string {
	p := strconv.Itoa(port)
	var plan [][]string
	for _, s := range snap.Services {
		plan = append(plan,
			[]string{"-setwebproxy", s.Name, "127.0.0.1", p},
			[]string{"-setsecurewebproxy", s.Name, "127.0.0.1", p},
			[]string{"-setsocksfirewallproxy", s.Name, "127.0.0.1", p},
			append([]string{"-setproxybypassdomains", s.Name}, bypass...),
		)
		if s.AutoURLEnabled {
			plan = append(plan, []string{"-setautoproxystate", s.Name, "off"})
		}
		if s.AutoDiscovery {
			plan = append(plan, []string{"-setproxyautodiscovery", s.Name, "off"})
		}
	}
	return plan
}

// RestorePlan returns the argv lists that write back the recorded state, but
// only what Apply changed: the three proxies (always), the bypass list (only
// when it differs from appliedBypass; a nil appliedBypass means unknown and
// always writes), the PAC URL and state (only when PAC was on) and WPAD (only
// when it was on).
func RestorePlan(snap Snapshot, appliedBypass []string) [][]string {
	var plan [][]string
	for _, s := range snap.Services {
		plan = append(plan, restoreProxy(s.Name, "-setwebproxy", "-setwebproxystate", s.Web)...)
		plan = append(plan, restoreProxy(s.Name, "-setsecurewebproxy", "-setsecurewebproxystate", s.Secure)...)
		plan = append(plan, restoreProxy(s.Name, "-setsocksfirewallproxy", "-setsocksfirewallproxystate", s.Socks)...)
		if appliedBypass == nil || !equalStrings(s.Bypass, appliedBypass) {
			if len(s.Bypass) == 0 {
				plan = append(plan, []string{"-setproxybypassdomains", s.Name, "Empty"})
			} else {
				plan = append(plan, append([]string{"-setproxybypassdomains", s.Name}, s.Bypass...))
			}
		}
		if s.AutoURLEnabled {
			if s.AutoURL != "" {
				plan = append(plan, []string{"-setautoproxyurl", s.Name, s.AutoURL})
			}
			plan = append(plan, []string{"-setautoproxystate", s.Name, "on"})
		}
		if s.AutoDiscovery {
			plan = append(plan, []string{"-setproxyautodiscovery", s.Name, "on"})
		}
	}
	return plan
}

func equalStrings(a, b []string) bool {
	if len(a) != len(b) {
		return false
	}
	for i := range a {
		if a[i] != b[i] {
			return false
		}
	}
	return true
}

func restoreProxy(service, setCmd, stateCmd string, st ProxyState) [][]string {
	var plan [][]string
	if st.Server != "" {
		plan = append(plan, []string{setCmd, service, st.Server, strconv.Itoa(st.Port)})
	} else {
		// Nothing was recorded: clear the 127.0.0.1:<port> Apply left in the
		// server fields instead of leaving a disabled entry behind.
		plan = append(plan, []string{setCmd, service, "", "0"})
	}
	return append(plan, []string{stateCmd, service, onOff(st.Enabled)})
}

const (
	maxBypassEntries = 64
	maxBypassLen     = 253
)

// ValidateBypass is deny-by-default: entries are host names, wildcards, IPs
// and CIDRs built from a small character set. Entries become argv elements
// of a root process, so anything else (whitespace, quotes, shell
// metacharacters, a leading '-') is rejected.
func ValidateBypass(list []string) error {
	if len(list) > maxBypassEntries {
		return fmt.Errorf("bypass list has %d entries, max %d", len(list), maxBypassEntries)
	}
	for i, tok := range list {
		if tok == "" {
			return fmt.Errorf("bypass entry %d is empty", i)
		}
		if len(tok) > maxBypassLen {
			return fmt.Errorf("bypass entry %d is longer than %d", i, maxBypassLen)
		}
		if tok[0] == '-' {
			return fmt.Errorf("bypass entry %d starts with '-'", i)
		}
		for _, c := range tok {
			switch {
			case c >= 'a' && c <= 'z', c >= 'A' && c <= 'Z', c >= '0' && c <= '9':
			case c == '.', c == '*', c == '/', c == ':', c == '_', c == '-', c == '[', c == ']':
			default:
				return fmt.Errorf("bypass entry %d contains forbidden character %q", i, c)
			}
		}
	}
	return nil
}

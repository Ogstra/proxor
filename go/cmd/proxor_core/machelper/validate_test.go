package machelper

import (
	"os"
	"path/filepath"
	"regexp"
	"runtime"
	"sort"
	"strings"
	"testing"

	"github.com/sagernet/sing-box/option"

	"proxor_core/boxmain"
)

const templatePath = "../../../../assets/res/vpn/sing-box-vpn.json"

// renderOpts mirrors the inputs of ConfigBuilder::WriteVPNSingBoxConfig.
type renderOpts struct {
	auth        bool
	processRule bool
	cidrRule    bool
	fakeDNS     bool
	ipv6        bool
	stack       string
	strict      bool
	domainDNS   bool
}

// renderTemplate reads the real GUI template and substitutes every placeholder
// the same way the C++ builder does (see the drift test below).
func renderTemplate(t *testing.T, o renderOpts) []byte {
	t.Helper()
	raw, err := os.ReadFile(templatePath)
	if err != nil {
		t.Fatalf("read template: %v", err)
	}
	addresses := `["172.19.0.1/28"]`
	if o.ipv6 {
		addresses = `["172.19.0.1/28","fdfe:dcba:9876::1/126"]`
	}
	remote := `{"tag":"dns-remote","type":"https","server":"8.8.8.8","path":"/dns-query","detour":"proxor-socks"}`
	if o.domainDNS {
		remote = `{"tag":"dns-remote","type":"https","server":"dns.google","path":"/dns-query","domain_resolver":"dns-local","detour":"proxor-socks"}`
	}
	auth, process, cidr := "", "", ""
	if o.auth {
		auth = ` "username": "u", "password": "p", `
	}
	if o.processRule {
		process = `,{"outbound":"direct","process_name":["Telegram"]}`
	}
	if o.cidrRule {
		cidr = `,{"outbound":"direct","ip_cidr":["192.168.0.0/16"]}`
	}
	fake := "empty"
	if o.fakeDNS {
		fake = "tun-in"
	}
	stack := o.stack
	if stack == "" {
		stack = "gvisor"
	}
	strict := "false"
	if o.strict {
		strict = "true"
	}
	r := strings.NewReplacer(
		"%TUN_ADDRESSES%", addresses,
		"%ROUTE_EXCLUDE_ADDRESSES%", `["10.0.0.0/8","13.107.4.52/32"]`,
		"%DNS_REMOTE_SERVER%", remote,
		"%DNS_DIRECT_SERVER%", `{"tag":"dns-direct","type":"local"}`,
		"%DNS_LOCAL_SERVER%", `{"tag":"dns-local","type":"local"}`,
		"//%SOCKS_USER_PASS%", auth,
		"//%PROCESS_NAME_RULE%", process,
		"//%CIDR_RULE%", cidr,
		"%MTU%", "9000",
		"%STACK%", stack,
		"%TUN_NAME%", "utun9",
		"%STRICT_ROUTE%", strict,
		"%FINAL_OUT%", "proxor-socks",
		"%FAKE_DNS_INBOUND%", fake,
		"%PORT%", "2080",
	)
	return []byte(r.Replace(string(raw)))
}

var goldenVariants = map[string]renderOpts{
	"defaults":     {},
	"everything":   {auth: true, processRule: true, cidrRule: true, fakeDNS: true, ipv6: true, domainDNS: true},
	"system-v6":    {ipv6: true, fakeDNS: true, stack: "system"},
	"mixed-strict": {auth: true, stack: "mixed", strict: true},
}

func tunOf(t *testing.T, opts option.Options) *option.TunInboundOptions {
	t.Helper()
	if len(opts.Inbounds) != 1 {
		t.Fatalf("expected 1 inbound, got %d", len(opts.Inbounds))
	}
	tun, ok := opts.Inbounds[0].Options.(*option.TunInboundOptions)
	if !ok {
		t.Fatalf("inbound options are %T", opts.Inbounds[0].Options)
	}
	return tun
}

func TestValidateGoldenTemplate(t *testing.T) {
	for name, o := range goldenVariants {
		t.Run(name, func(t *testing.T) {
			cfg := renderTemplate(t, o)
			ctx, opts, err := ValidateTunConfig(cfg, 2080)
			if err != nil {
				t.Fatalf("golden config rejected: %v\n%s", err, cfg)
			}
			if ctx == nil {
				t.Fatal("nil context")
			}
			tun := tunOf(t, opts)
			if tun.InterfaceName != "" {
				t.Fatalf("interface_name not cleared: %q", tun.InterfaceName)
			}
			if !tun.AutoRoute {
				t.Fatal("auto_route not forced")
			}
			locals := 0
			for _, s := range opts.DNS.Servers {
				if l, ok := s.Options.(*option.LocalDNSServerOptions); ok {
					locals++
					if !l.PreferGo {
						t.Fatalf("local dns server %q has PreferGo=false", s.Tag)
					}
				}
			}
			if locals == 0 {
				t.Fatal("no local dns server found in golden config")
			}
		})
	}
}

func TestValidateNormalizesHostileValues(t *testing.T) {
	cfg := string(renderTemplate(t, renderOpts{}))
	cfg = strings.Replace(cfg, `"auto_route": true`, `"auto_route": false`, 1)
	if !strings.Contains(cfg, `"auto_route": false`) {
		t.Fatal("test setup: auto_route not rewritten")
	}
	_, opts, err := ValidateTunConfig([]byte(cfg), 2080)
	if err != nil {
		t.Fatal(err)
	}
	tun := tunOf(t, opts)
	if !tun.AutoRoute || tun.InterfaceName != "" {
		t.Fatalf("not normalized: auto_route=%v interface_name=%q", tun.AutoRoute, tun.InterfaceName)
	}
}

func TestValidatePlaceholderDrift(t *testing.T) {
	raw, err := os.ReadFile(templatePath)
	if err != nil {
		t.Fatal(err)
	}
	re := regexp.MustCompile(`%[A-Z0-9_]+%`)
	found := map[string]bool{}
	for _, m := range re.FindAllString(string(raw), -1) {
		found[m] = true
	}
	// Placeholders the renderer above substitutes.
	handled := []string{
		"%TUN_ADDRESSES%", "%ROUTE_EXCLUDE_ADDRESSES%", "%DNS_REMOTE_SERVER%", "%DNS_DIRECT_SERVER%",
		"%DNS_LOCAL_SERVER%", "%SOCKS_USER_PASS%", "%PROCESS_NAME_RULE%", "%CIDR_RULE%", "%MTU%",
		"%STACK%", "%TUN_NAME%", "%STRICT_ROUTE%", "%FINAL_OUT%", "%FAKE_DNS_INBOUND%", "%PORT%",
	}
	want := map[string]bool{}
	for _, h := range handled {
		want[h] = true
	}
	var missing, extra []string
	for k := range found {
		if !want[k] {
			missing = append(missing, k)
		}
	}
	for k := range want {
		if !found[k] {
			extra = append(extra, k)
		}
	}
	sort.Strings(missing)
	sort.Strings(extra)
	if len(missing) > 0 || len(extra) > 0 {
		t.Fatalf("template placeholders drifted: not handled by renderer=%v, no longer in template=%v", missing, extra)
	}
}

// hostileKey lists, per corpus file, the key the error message must name.
var hostileKey = map[string]string{
	"experimental":                "experimental",
	"endpoints":                   "endpoints",
	"services":                    "services",
	"certificate":                 "certificate",
	"ntp":                         "ntp",
	"log-output":                  "log.output",
	"rule_set":                    "rule_set",
	"route_address_set":           "route_address_set",
	"socks-nonloopback":           "socks",
	"two-inbounds":                "inbounds",
	"inbound-not-tun":             "inbound",
	"outbound-type":               "outbound",
	"dial-bind":                   "bind_interface",
	"tun-platform":                "platform",
	"tun-auto_redirect":           "auto_redirect",
	"dns-hosts":                   "hosts",
	"dns-tls-cert":                "certificate_path",
	"detour-unknown":              "detour",
	"dns-dhcp":                    "dhcp",
	"dns-rule_set":                "rule_set",
	"dns-domain_resolver-unknown": "domain_resolver",
	"route-final-unknown":         "final",
	"route-rule_set":              "rule_set",
	"dns-https-path":              "path",
	"route-default_interface":     "default_interface",
}

func TestValidateHostile(t *testing.T) {
	files, err := filepath.Glob("testdata/hostile/*.json")
	if err != nil {
		t.Fatal(err)
	}
	if len(files) < 18 {
		t.Fatalf("hostile corpus too small: %d files", len(files))
	}
	seen := map[string]bool{}
	for _, file := range files {
		name := strings.TrimSuffix(filepath.Base(file), ".json")
		seen[name] = true
		t.Run(name, func(t *testing.T) {
			key, ok := hostileKey[name]
			if !ok {
				t.Fatalf("hostile file %s has no expected key in hostileKey", name)
			}
			data, err := os.ReadFile(file)
			if err != nil {
				t.Fatal(err)
			}
			_, _, err = ValidateTunConfig(data, 2080)
			if err == nil {
				t.Fatalf("hostile config %s was accepted", name)
			}
			if !strings.Contains(err.Error(), key) {
				t.Fatalf("error %q does not name %q", err.Error(), key)
			}
		})
	}
	for name := range hostileKey {
		if !seen[name] {
			t.Errorf("hostileKey lists %s but testdata/hostile/%s.json is missing", name, name)
		}
	}
}

func TestValidateSocksPortMismatch(t *testing.T) {
	cfg := renderTemplate(t, renderOpts{})
	_, _, err := ValidateTunConfig(cfg, 9999)
	if err == nil || !strings.Contains(err.Error(), "socks") {
		t.Fatalf("expected a socks port error, got %v", err)
	}
	for _, bad := range []int{0, -1, 70000} {
		if _, _, err := ValidateTunConfig(cfg, bad); err == nil {
			t.Fatalf("socksPort %d accepted", bad)
		}
	}
}

func TestValidateRouteAddressAllowed(t *testing.T) {
	cfg := strings.Replace(string(renderTemplate(t, renderOpts{})),
		`"route_exclude_address"`, `"route_address": ["198.51.100.0/24"], "route_exclude_address"`, 1)
	if !strings.Contains(cfg, "198.51.100.0/24") {
		t.Fatal("test setup: route_address not injected")
	}
	_, opts, err := ValidateTunConfig([]byte(cfg), 2080)
	if err != nil {
		t.Fatalf("route_address rejected: %v", err)
	}
	if len(tunOf(t, opts).RouteAddress) != 1 {
		t.Fatal("route_address lost")
	}
}

func TestValidateRejectsGarbage(t *testing.T) {
	for _, in := range []string{"", "not json", "[]", `{"inbounds":`, `{"unknown_top_level":1}`} {
		if _, _, err := ValidateTunConfig([]byte(in), 2080); err == nil {
			t.Fatalf("garbage %q accepted", in)
		}
	}
}

func TestValidatedOptionsCreate(t *testing.T) {
	if runtime.GOOS != "darwin" {
		t.Skip("instance creation is only exercised on darwin")
	}
	ctx, opts, err := ValidateTunConfig(renderTemplate(t, renderOpts{}), 2080)
	if err != nil {
		t.Fatal(err)
	}
	if err := boxmain.CheckOptions(ctx, opts); err != nil {
		t.Fatalf("normalized options do not create: %v", err)
	}
}

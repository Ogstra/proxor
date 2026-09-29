package machelper

import (
	"os"
	"path/filepath"
	"reflect"
	"strings"
	"testing"
)

func fixture(t *testing.T, name string) string {
	t.Helper()
	data, err := os.ReadFile(filepath.Join("testdata", "networksetup", name))
	if err != nil {
		t.Fatal(err)
	}
	return string(data)
}

func TestParseServiceOrder(t *testing.T) {
	got, err := ParseServiceOrder(fixture(t, "listnetworkserviceorder.txt"))
	if err != nil {
		t.Fatal(err)
	}
	if len(got) != 10 {
		t.Fatalf("want 10 services, got %d: %+v", len(got), got)
	}
	want := map[string]string{
		"USB 10/100/1000 LAN": "en10",
		"Thunderbolt Bridge":  "bridge0",
		"Wi-Fi":               "en0",
		"iPhone USB":          "en11",
		"Happ":                "",
		"Tailscale":           "",
		"Shadowrocket":        "",
	}
	byName := map[string]NetworkService{}
	for _, s := range got {
		byName[s.Name] = s
	}
	for name, dev := range want {
		s, ok := byName[name]
		if !ok {
			t.Fatalf("service %q missing", name)
		}
		if s.Device != dev {
			t.Errorf("service %q device = %q, want %q", name, s.Device, dev)
		}
	}
}

func TestParseServiceOrderDisabledMarkerAndSlash(t *testing.T) {
	got, err := ParseServiceOrder(fixture(t, "listnetworkserviceorder-disabled.txt"))
	if err != nil {
		t.Fatal(err)
	}
	want := []NetworkService{
		{Name: "Wi-Fi", Device: "en0"},
		{Name: "Old Dock/Hub", Device: "en7", Disabled: true},
		{Name: "AX88179 USB 3.0/2.0 LAN", Device: "en8"},
	}
	if !reflect.DeepEqual(got, want) {
		t.Fatalf("got %+v want %+v", got, want)
	}
}

func TestParseAllServices(t *testing.T) {
	got, err := ParseAllServices(fixture(t, "listallnetworkservices-disabled.txt"))
	if err != nil {
		t.Fatal(err)
	}
	want := []NetworkService{
		{Name: "Wi-Fi"},
		{Name: "Old Dock/Hub", Disabled: true},
		{Name: "AX88179 USB 3.0/2.0 LAN"},
	}
	if !reflect.DeepEqual(got, want) {
		t.Fatalf("got %+v want %+v", got, want)
	}
	real, err := ParseAllServices(fixture(t, "listallnetworkservices.txt"))
	if err != nil {
		t.Fatal(err)
	}
	if len(real) != 10 {
		t.Fatalf("real fixture: want 10 services (header skipped), got %d: %+v", len(real), real)
	}
}

func TestEligibleServices(t *testing.T) {
	order, _ := ParseServiceOrder(fixture(t, "listnetworkserviceorder.txt"))
	all, _ := ParseAllServices(fixture(t, "listallnetworkservices.txt"))
	got := EligibleServices(order, all)
	var names []string
	for _, s := range got {
		names = append(names, s.Name)
	}
	want := []string{"USB 10/100/1000 LAN", "Thunderbolt Bridge", "Wi-Fi", "iPhone USB"}
	if !reflect.DeepEqual(names, want) {
		t.Fatalf("eligible = %v, want %v (NE VPN services with empty Device must be left alone)", names, want)
	}

	// disabled per -listallnetworkservices
	order2, _ := ParseServiceOrder(fixture(t, "listnetworkserviceorder-disabled.txt"))
	all2, _ := ParseAllServices(fixture(t, "listallnetworkservices-disabled.txt"))
	got2 := EligibleServices(order2, all2)
	names = nil
	for _, s := range got2 {
		names = append(names, s.Name)
	}
	if !reflect.DeepEqual(names, []string{"Wi-Fi", "AX88179 USB 3.0/2.0 LAN"}) {
		t.Fatalf("eligible (disabled) = %v", names)
	}

	// disabled only per the allnetworkservices asterisk, order says nothing
	order3 := []NetworkService{{Name: "A", Device: "en0"}, {Name: "B", Device: "en1"}}
	all3 := []NetworkService{{Name: "A"}, {Name: "B", Disabled: true}}
	got3 := EligibleServices(order3, all3)
	if len(got3) != 1 || got3[0].Name != "A" {
		t.Fatalf("eligible (asterisk) = %+v", got3)
	}
}

func TestParseProxyState(t *testing.T) {
	off, err := ParseProxyState(fixture(t, "webproxy-off.txt"))
	if err != nil {
		t.Fatal(err)
	}
	if off != (ProxyState{}) {
		t.Fatalf("off = %+v", off)
	}
	on, err := ParseProxyState(fixture(t, "webproxy-on.txt"))
	if err != nil {
		t.Fatal(err)
	}
	if on != (ProxyState{Enabled: true, Server: "10.0.0.1", Port: 3128}) {
		t.Fatalf("on = %+v", on)
	}
	if _, err := ParseProxyState("garbage\n"); err == nil {
		t.Fatal("garbage must be an error")
	}
	if _, err := ParseProxyState(fixture(t, "error-nosuch.txt")); err == nil {
		t.Fatal("** Error: must be an error")
	}
}

func TestParseBypassDomains(t *testing.T) {
	none, err := ParseBypassDomains(fixture(t, "bypass-none-happ.txt"))
	if err != nil {
		t.Fatal(err)
	}
	if len(none) != 0 {
		t.Fatalf("none = %v", none)
	}
	tb, err := ParseBypassDomains(fixture(t, "bypass-thunderbolt.txt"))
	if err != nil {
		t.Fatal(err)
	}
	if !reflect.DeepEqual(tb, []string{"*.local", "169.254/16"}) {
		t.Fatalf("tb = %v", tb)
	}
	if _, err := ParseBypassDomains(fixture(t, "error-nosuch.txt")); err == nil {
		t.Fatal("** Error: must be an error")
	}
}

func TestParseAutoProxyURL(t *testing.T) {
	null, err := ParseAutoProxyURL(fixture(t, "autoproxyurl-null.txt"))
	if err != nil {
		t.Fatal(err)
	}
	if null != (AutoProxy{}) {
		t.Fatalf("null = %+v", null)
	}
	set, err := ParseAutoProxyURL(fixture(t, "autoproxyurl-set.txt"))
	if err != nil {
		t.Fatal(err)
	}
	if set != (AutoProxy{URL: "http://wpad.example/proxy.pac", Enabled: true}) {
		t.Fatalf("set = %+v", set)
	}
	if _, err := ParseAutoProxyURL(fixture(t, "error-nosuch.txt")); err == nil {
		t.Fatal("** Error: must be an error")
	}
}

func TestParseAutoDiscovery(t *testing.T) {
	on, err := ParseAutoDiscovery(fixture(t, "autodiscovery-on.txt"))
	if err != nil || !on {
		t.Fatalf("on = %v, %v", on, err)
	}
	off, err := ParseAutoDiscovery(fixture(t, "autodiscovery-off.txt"))
	if err != nil || off {
		t.Fatalf("off = %v, %v", off, err)
	}
	if _, err := ParseAutoDiscovery(fixture(t, "error-nosuch.txt")); err == nil {
		t.Fatal("** Error: must be an error")
	}
	if _, err := ParseAutoDiscovery("nonsense"); err == nil {
		t.Fatal("nonsense must be an error")
	}
}

func testSnapshot() Snapshot {
	return Snapshot{
		Applied: true,
		Services: []ServiceSnapshot{
			{
				Name:           "Wi-Fi",
				Web:            ProxyState{},
				Secure:         ProxyState{Enabled: true, Server: "10.0.0.1", Port: 3128},
				Socks:          ProxyState{},
				Bypass:         []string{"*.local", "169.254/16"},
				AutoURL:        "http://wpad.example/proxy.pac",
				AutoURLEnabled: true,
				AutoDiscovery:  true,
			},
			{Name: "USB 10/100/1000 LAN"},
		},
	}
}

func TestApplyPlan(t *testing.T) {
	bypass := []string{"127.0.0.1", "localhost", "*.local"}
	got := ApplyPlan(testSnapshot(), 2080, bypass)
	want := [][]string{
		{"-setwebproxy", "Wi-Fi", "127.0.0.1", "2080"},
		{"-setsecurewebproxy", "Wi-Fi", "127.0.0.1", "2080"},
		{"-setsocksfirewallproxy", "Wi-Fi", "127.0.0.1", "2080"},
		{"-setproxybypassdomains", "Wi-Fi", "127.0.0.1", "localhost", "*.local"},
		{"-setautoproxystate", "Wi-Fi", "off"},
		{"-setproxyautodiscovery", "Wi-Fi", "off"},
		// second service: PAC/WPAD were not on, so they are not touched
		{"-setwebproxy", "USB 10/100/1000 LAN", "127.0.0.1", "2080"},
		{"-setsecurewebproxy", "USB 10/100/1000 LAN", "127.0.0.1", "2080"},
		{"-setsocksfirewallproxy", "USB 10/100/1000 LAN", "127.0.0.1", "2080"},
		{"-setproxybypassdomains", "USB 10/100/1000 LAN", "127.0.0.1", "localhost", "*.local"},
	}
	if !reflect.DeepEqual(got, want) {
		t.Fatalf("ApplyPlan mismatch\n got: %q\nwant: %q", got, want)
	}
}

func TestApplyPlanPerServiceIsSingleServiceArgv(t *testing.T) {
	for _, argv := range ApplyPlan(testSnapshot(), 2080, DefaultBypass()) {
		if argv[1] != "Wi-Fi" && argv[1] != "USB 10/100/1000 LAN" {
			t.Fatalf("service name split or missing in %q", argv)
		}
	}
}

var testAppliedBypass = []string{"127.0.0.1", "localhost", "*.local"}

func TestRestorePlan(t *testing.T) {
	got := RestorePlan(testSnapshot(), testAppliedBypass)
	want := [][]string{
		// Wi-Fi: web had no server recorded -> only the state; secure had one -> server+port then state
		{"-setwebproxystate", "Wi-Fi", "off"},
		{"-setsecurewebproxy", "Wi-Fi", "10.0.0.1", "3128"},
		{"-setsecurewebproxystate", "Wi-Fi", "on"},
		{"-setsocksfirewallproxystate", "Wi-Fi", "off"},
		{"-setproxybypassdomains", "Wi-Fi", "*.local", "169.254/16"},
		{"-setautoproxyurl", "Wi-Fi", "http://wpad.example/proxy.pac"},
		{"-setautoproxystate", "Wi-Fi", "on"},
		{"-setproxyautodiscovery", "Wi-Fi", "on"},
		// USB: everything off, PAC/WPAD were off so Apply never touched them,
		// bypass list recorded empty -> Empty keyword
		{"-setwebproxystate", "USB 10/100/1000 LAN", "off"},
		{"-setsecurewebproxystate", "USB 10/100/1000 LAN", "off"},
		{"-setsocksfirewallproxystate", "USB 10/100/1000 LAN", "off"},
		{"-setproxybypassdomains", "USB 10/100/1000 LAN", "Empty"},
	}
	if !reflect.DeepEqual(got, want) {
		t.Fatalf("RestorePlan mismatch\n got: %q\nwant: %q", got, want)
	}
}

func TestRestorePlanEmptyBypassUsesEmptyKeyword(t *testing.T) {
	found := false
	for _, argv := range RestorePlan(Snapshot{Services: []ServiceSnapshot{{Name: "X"}}}, testAppliedBypass) {
		if argv[0] == "-setproxybypassdomains" {
			found = true
			if !reflect.DeepEqual(argv, []string{"-setproxybypassdomains", "X", "Empty"}) {
				t.Fatalf("argv = %q", argv)
			}
		}
	}
	if !found {
		t.Fatal("no bypass restore emitted")
	}
}

func planHas(plan [][]string, cmd string) bool {
	for _, a := range plan {
		if a[0] == cmd {
			return true
		}
	}
	return false
}

func TestApplyPlanNoRedundantStateOn(t *testing.T) {
	for _, argv := range ApplyPlan(testSnapshot(), 2080, DefaultBypass()) {
		switch argv[0] {
		case "-setwebproxystate", "-setsecurewebproxystate", "-setsocksfirewallproxystate":
			t.Fatalf("redundant state write %q (the set* commands already turn the proxy on)", argv)
		}
	}
	plan := ApplyPlan(Snapshot{Services: []ServiceSnapshot{{Name: "P", AutoURLEnabled: true, AutoDiscovery: true}}}, 1, DefaultBypass())
	if !planHas(plan, "-setproxybypassdomains") || !planHas(plan, "-setautoproxystate") || !planHas(plan, "-setproxyautodiscovery") {
		t.Fatalf("bypass and PAC/WPAD-off entries must stay: %q", plan)
	}
}

func TestRestorePlanSkipsUntouched(t *testing.T) {
	applied := DefaultBypass()

	// everything off, recorded bypass equal to the applied one
	off := RestorePlan(Snapshot{AppliedBypass: applied, Services: []ServiceSnapshot{{Name: "S", Bypass: DefaultBypass()}}}, applied)
	for _, c := range []string{"-setautoproxyurl", "-setautoproxystate", "-setproxyautodiscovery", "-setproxybypassdomains"} {
		if planHas(off, c) {
			t.Fatalf("%s must be skipped for an untouched service: %q", c, off)
		}
	}
	if len(off) != 3 {
		t.Fatalf("all-off service = %d writes, want 3 (the state triple): %q", len(off), off)
	}

	// recorded servers add up to 3 set writes on top of the states
	srv := ServiceSnapshot{Name: "S", Web: ProxyState{Server: "1.1.1.1", Port: 1}, Secure: ProxyState{Server: "1.1.1.1", Port: 1}, Socks: ProxyState{Server: "1.1.1.1", Port: 1}, Bypass: []string{"a"}}
	if got := RestorePlan(Snapshot{Services: []ServiceSnapshot{srv}}, applied); len(got) != 7 {
		t.Fatalf("all-off + servers + differing bypass = %d writes, want 7: %q", len(got), got)
	}

	// PAC on: URL then state on
	pac := RestorePlan(Snapshot{Services: []ServiceSnapshot{{Name: "S", AutoURL: "http://p/x.pac", AutoURLEnabled: true, Bypass: applied}}}, applied)
	if !reflect.DeepEqual(pac[len(pac)-2:], [][]string{{"-setautoproxyurl", "S", "http://p/x.pac"}, {"-setautoproxystate", "S", "on"}}) {
		t.Fatalf("PAC restore = %q", pac)
	}
	if planHas(pac, "-setproxyautodiscovery") {
		t.Fatalf("WPAD was off: %q", pac)
	}

	// PAC configured but disabled: Apply never turned it off, so nothing to write
	if got := RestorePlan(Snapshot{Services: []ServiceSnapshot{{Name: "S", AutoURL: "http://p/x.pac", Bypass: applied}}}, applied); planHas(got, "-setautoproxyurl") || planHas(got, "-setautoproxystate") {
		t.Fatalf("disabled PAC must not be written: %q", got)
	}

	// WPAD on (the owner's Wi-Fi)
	wpad := RestorePlan(Snapshot{Services: []ServiceSnapshot{{Name: "S", AutoDiscovery: true, Bypass: applied}}}, applied)
	if !reflect.DeepEqual(wpad[len(wpad)-1], []string{"-setproxyautodiscovery", "S", "on"}) || planHas(wpad, "-setautoproxystate") {
		t.Fatalf("WPAD restore = %q", wpad)
	}
}

func TestRestorePlanBypassNilVersusEmpty(t *testing.T) {
	svc := ServiceSnapshot{Name: "S", Bypass: []string{}}
	// old snapshot without the applied list (nil): restore bypass unconditionally, also when empty
	if got := RestorePlan(Snapshot{Services: []ServiceSnapshot{svc}}, nil); !planHas(got, "-setproxybypassdomains") {
		t.Fatalf("nil applied bypass must always write the bypass: %q", got)
	}
	svc.Bypass = DefaultBypass()
	if got := RestorePlan(Snapshot{Services: []ServiceSnapshot{svc}}, nil); !planHas(got, "-setproxybypassdomains") {
		t.Fatalf("nil applied bypass must always write the bypass: %q", got)
	}
	// non-nil applied list equal to the recorded one: skipped
	if got := RestorePlan(Snapshot{Services: []ServiceSnapshot{svc}}, DefaultBypass()); planHas(got, "-setproxybypassdomains") {
		t.Fatalf("equal lists must skip the write: %q", got)
	}
	// order-sensitive compare
	rev := append([]string(nil), DefaultBypass()...)
	rev[0], rev[1] = rev[1], rev[0]
	if got := RestorePlan(Snapshot{Services: []ServiceSnapshot{svc}}, rev); !planHas(got, "-setproxybypassdomains") {
		t.Fatalf("a different order must write: %q", got)
	}
}

func TestDefaultBypass(t *testing.T) {
	want := []string{"127.0.0.1", "localhost", "*.local", "169.254.0.0/16", "10.0.0.0/8", "172.16.0.0/12", "192.168.0.0/16", "100.64.0.0/10"}
	if !reflect.DeepEqual(DefaultBypass(), want) {
		t.Fatalf("DefaultBypass = %v", DefaultBypass())
	}
	if err := ValidateBypass(DefaultBypass()); err != nil {
		t.Fatal(err)
	}
}

func TestValidateBypass(t *testing.T) {
	ok := strings.Fields("127.0.0.1 localhost *.local 169.254.0.0/16 10.0.0.0/8 172.16.0.0/12 192.168.0.0/16 100.64.0.0/10 169.254/16 example.com")
	if err := ValidateBypass(ok); err != nil {
		t.Fatalf("valid list rejected: %v", err)
	}
	bad := map[string]string{
		"space":        "a b",
		"tab":          "a\tb",
		"newline":      "a\nb",
		"dquote":       `a"b`,
		"squote":       "a'b",
		"semicolon":    "a;b",
		"dollar":       "$HOME",
		"backtick":     "`id`",
		"leading dash": "-setwebproxy",
		"empty":        "",
		"too long":     strings.Repeat("a", 254),
		"pipe":         "a|b",
		"backslash":    `a\b`,
	}
	for name, tok := range bad {
		if err := ValidateBypass([]string{tok}); err == nil {
			t.Errorf("%s: token %q must be rejected", name, tok)
		}
	}
	if err := ValidateBypass([]string{strings.Repeat("a", 253)}); err != nil {
		t.Errorf("253 chars must be accepted: %v", err)
	}
	many := make([]string, 65)
	for i := range many {
		many[i] = "example.com"
	}
	if err := ValidateBypass(many); err == nil {
		t.Error("65 entries must be rejected")
	}
	if err := ValidateBypass(many[:64]); err != nil {
		t.Errorf("64 entries must be accepted: %v", err)
	}
}

func TestSysproxyPlanIsPure(t *testing.T) {
	src, err := os.ReadFile("sysproxy_plan.go")
	if err != nil {
		t.Fatal(err)
	}
	if strings.Contains(string(src), "exec.") || strings.Contains(string(src), "\"os/exec\"") {
		t.Fatal("sysproxy_plan.go must stay pure (no process execution)")
	}
}

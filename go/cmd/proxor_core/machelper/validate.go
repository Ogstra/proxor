package machelper

import (
	"context"
	"errors"
	"fmt"
	"reflect"
	"sort"
	"strings"

	C "github.com/sagernet/sing-box/constant"
	"github.com/sagernet/sing-box/option"

	"proxor_core/boxmain"
)

// ValidateTunConfig is the trust boundary of the root helper (MAC-TUN-03).
//
// The helper runs sing-box as root from JSON a user-level process sends, so the
// config is parsed with sing-box's own registry-aware decoder (no parser
// differential) and then walked field by field, deny by default: anything the
// GUI's Tun template does not need is rejected with a reason naming the key.
// The returned options are normalized (interface_name cleared so sing-box picks
// a free utunN, auto_route forced, `local` DNS servers get prefer_go) and are
// what the caller must run; it must never re-parse the raw bytes.
func ValidateTunConfig(config []byte, socksPort int) (context.Context, option.Options, error) {
	if socksPort < 1 || socksPort > 65535 {
		return nil, option.Options{}, fmt.Errorf("invalid socksPort %d", socksPort)
	}
	ctx, opts, err := boxmain.ParseOptions(config)
	if err != nil {
		return nil, option.Options{}, err
	}
	if err := validateOptions(&opts, socksPort); err != nil {
		return nil, option.Options{}, err
	}
	return ctx, opts, nil
}

func validateOptions(opts *option.Options, socksPort int) error {
	if opts.NTP != nil {
		return errors.New(`"ntp" is not allowed`)
	}
	if opts.Certificate != nil {
		return errors.New(`"certificate" is not allowed`)
	}
	if len(opts.Endpoints) > 0 {
		return errors.New(`"endpoints" is not allowed`)
	}
	if len(opts.Services) > 0 {
		return errors.New(`"services" is not allowed`)
	}
	if opts.Experimental != nil {
		return errors.New(`"experimental" is not allowed`)
	}
	if opts.Log != nil && opts.Log.Output != "" {
		return errors.New(`"log.output" is not allowed`)
	}

	outboundTags, err := validateOutbounds(opts.Outbounds, socksPort)
	if err != nil {
		return err
	}
	if err := validateInbounds(opts.Inbounds); err != nil {
		return err
	}
	dnsTags, err := validateDNS(opts.DNS, outboundTags)
	if err != nil {
		return err
	}
	return validateRoute(opts.Route, outboundTags, dnsTags)
}

// ---- generic deny-by-default field walker ------------------------------------

// extraKeys returns the JSON keys of every non-empty field of v (a struct or a
// pointer to one, embedded structs flattened) that is not listed in allowed.
// Fields tagged json:"-" are internal and ignored. Because it walks the real
// sing-box option structs, a field added by a future sing-box bump is denied
// until it is explicitly allowed here.
func extraKeys(v any, allowed ...string) []string {
	allow := make(map[string]bool, len(allowed))
	for _, k := range allowed {
		allow[k] = true
	}
	var found []string
	collectKeys(reflect.ValueOf(v), allow, &found)
	sort.Strings(found)
	return found
}

func collectKeys(v reflect.Value, allow map[string]bool, out *[]string) {
	for v.Kind() == reflect.Pointer || v.Kind() == reflect.Interface {
		if v.IsNil() {
			return
		}
		v = v.Elem()
	}
	if v.Kind() != reflect.Struct {
		return
	}
	t := v.Type()
	for i := 0; i < t.NumField(); i++ {
		f := t.Field(i)
		if f.PkgPath != "" {
			continue
		}
		tag, hasTag := f.Tag.Lookup("json")
		name, _, _ := strings.Cut(tag, ",")
		if name == "-" {
			continue
		}
		fv := v.Field(i)
		if f.Anonymous && (!hasTag || name == "") {
			collectKeys(fv, allow, out)
			continue
		}
		if name == "" {
			name = f.Name
		}
		if isEmptyValue(fv) || allow[name] {
			continue
		}
		*out = append(*out, name)
	}
}

func isEmptyValue(v reflect.Value) bool {
	switch v.Kind() {
	case reflect.Slice, reflect.Map:
		return v.Len() == 0
	case reflect.Pointer, reflect.Interface:
		return v.IsNil()
	default:
		return v.IsZero()
	}
}

func denyKeys(where string, v any, allowed ...string) error {
	if keys := extraKeys(v, allowed...); len(keys) > 0 {
		return fmt.Errorf("%s: field %q is not allowed", where, keys[0])
	}
	return nil
}

func nonEmpty(list []string) bool { return len(list) > 0 }

// ---- outbounds ----------------------------------------------------------------

func validateOutbounds(outbounds []option.Outbound, socksPort int) (map[string]bool, error) {
	tags := make(map[string]bool, len(outbounds))
	socks := 0
	for _, out := range outbounds {
		if out.Tag == "" {
			return nil, fmt.Errorf("outbound of type %q has no tag", out.Type)
		}
		if tags[out.Tag] {
			return nil, fmt.Errorf("outbound %q: duplicate tag", out.Tag)
		}
		tags[out.Tag] = true
		where := fmt.Sprintf("outbound %q", out.Tag)
		switch out.Type {
		case C.TypeSOCKS:
			o, ok := out.Options.(*option.SOCKSOutboundOptions)
			if !ok {
				return nil, fmt.Errorf("%s: unexpected options type %T", where, out.Options)
			}
			// Only server, auth and version; every dial field except udp_fragment is denied
			// (bind_interface, detour, routing_mark, netns, ... would let the config steer root traffic).
			if err := denyKeys(where+" (socks)", o, "server", "server_port", "version", "username", "password", "udp_fragment"); err != nil {
				return nil, err
			}
			if o.Server != "127.0.0.1" && o.Server != "::1" {
				return nil, fmt.Errorf("%s: socks server must be 127.0.0.1 or ::1, got %q", where, o.Server)
			}
			if int(o.ServerPort) != socksPort {
				return nil, fmt.Errorf("%s: socks server_port %d does not match the core port %d", where, o.ServerPort, socksPort)
			}
			socks++
		case C.TypeDirect:
			o, ok := out.Options.(*option.DirectOutboundOptions)
			if !ok {
				return nil, fmt.Errorf("%s: unexpected options type %T", where, out.Options)
			}
			if err := denyKeys(where+" (direct)", o, "udp_fragment"); err != nil {
				return nil, err
			}
		default:
			return nil, fmt.Errorf("%s: outbound type %q is not allowed (only socks and direct)", where, out.Type)
		}
	}
	if socks == 0 {
		return nil, errors.New("outbounds: a socks outbound to the core is required")
	}
	return tags, nil
}

// ---- inbounds -----------------------------------------------------------------

func validateInbounds(inbounds []option.Inbound) error {
	if len(inbounds) != 1 {
		return fmt.Errorf("exactly one tun inbound is required, got %d inbounds", len(inbounds))
	}
	in := &inbounds[0]
	if in.Type != C.TypeTun {
		return fmt.Errorf("inbound %q: type %q is not allowed (only tun)", in.Tag, in.Type)
	}
	tun, ok := in.Options.(*option.TunInboundOptions)
	if !ok {
		return fmt.Errorf("inbound %q: unexpected options type %T", in.Tag, in.Options)
	}
	where := fmt.Sprintf("inbound %q (tun)", in.Tag)
	// auto_redirect*, iproute2_*, *_address_set, include_*/exclude_*, loopback_address, platform and
	// the deprecated inet4_/inet6_ fields are all denied by omission.
	if err := denyKeys(where, tun,
		"interface_name", "mtu", "address", "auto_route", "strict_route",
		"route_address", "route_exclude_address", "stack", "endpoint_independent_nat", "udp_timeout",
		"sniff", "sniff_override_destination", "sniff_timeout", "domain_strategy", "udp_disable_domain_unmapping",
	); err != nil {
		return err
	}
	switch tun.Stack {
	case "", "system", "gvisor", "mixed":
	default:
		return fmt.Errorf("%s: stack %q is not allowed", where, tun.Stack)
	}
	// Normalization: sing-box picks the next free utunN, and the helper always installs routes.
	tun.InterfaceName = ""
	tun.AutoRoute = true
	return nil
}

// ---- DNS ----------------------------------------------------------------------

func validateDNS(dns *option.DNSOptions, outboundTags map[string]bool) (map[string]bool, error) {
	tags := map[string]bool{}
	if dns == nil {
		return tags, nil
	}
	if dns.FakeIP != nil {
		return nil, errors.New(`dns: legacy "fakeip" block is not allowed`)
	}
	for i := range dns.Servers {
		if tag := dns.Servers[i].Tag; tag != "" {
			if tags[tag] {
				return nil, fmt.Errorf("dns server %q: duplicate tag", tag)
			}
			tags[tag] = true
		}
	}
	for i := range dns.Servers {
		if err := validateDNSServer(&dns.Servers[i], tags, outboundTags); err != nil {
			return nil, err
		}
	}
	if dns.Final != "" && !tags[dns.Final] {
		return nil, fmt.Errorf("dns.final %q is not a dns server tag", dns.Final)
	}
	if err := validateDNSRules(dns.Rules, tags); err != nil {
		return nil, err
	}
	return tags, nil
}

var dnsDialKeys = []string{"detour", "domain_resolver", "udp_fragment"}

func validateDNSServer(s *option.DNSServerOptions, dnsTags, outboundTags map[string]bool) error {
	where := fmt.Sprintf("dns server %q", s.Tag)
	switch s.Type {
	case C.DNSTypeLocal, C.DNSTypeUDP, C.DNSTypeTCP, C.DNSTypeTLS, C.DNSTypeHTTPS,
		C.DNSTypeQUIC, C.DNSTypeHTTP3, C.DNSTypeFakeIP:
	default:
		// hosts (file path), dhcp, tailscale, resolved, unupgraded legacy forms, ...
		return fmt.Errorf("%s: dns server type %q is not allowed", where, s.Type)
	}
	var dial *option.DialerOptions
	switch o := s.Options.(type) {
	case *option.LocalDNSServerOptions:
		if err := denyKeys(where, o, append([]string{"prefer_go"}, dnsDialKeys...)...); err != nil {
			return err
		}
		// sing-box #3359: the cgo-less resolver path misbehaves under Tun on macOS.
		o.PreferGo = true
		dial = &o.DialerOptions
	case *option.RemoteDNSServerOptions:
		if err := denyKeys(where, o, append([]string{"server", "server_port"}, dnsDialKeys...)...); err != nil {
			return err
		}
		dial = &o.DialerOptions
	case *option.RemoteTLSDNSServerOptions:
		if err := denyKeys(where, o, append([]string{"server", "server_port", "tls"}, dnsDialKeys...)...); err != nil {
			return err
		}
		if err := validateTLS(where, o.TLS); err != nil {
			return err
		}
		dial = &o.DialerOptions
	case *option.RemoteHTTPSDNSServerOptions:
		if err := denyKeys(where, o, append([]string{"server", "server_port", "tls", "path", "method", "headers"}, dnsDialKeys...)...); err != nil {
			return err
		}
		if o.Path != "" && !strings.HasPrefix(o.Path, "/") {
			return fmt.Errorf("%s: path %q must be a URL path starting with /", where, o.Path)
		}
		if err := validateTLS(where, o.TLS); err != nil {
			return err
		}
		dial = &o.DialerOptions
	case *option.FakeIPDNSServerOptions:
		return denyKeys(where, o, "inet4_range", "inet6_range")
	default:
		return fmt.Errorf("%s: unexpected options type %T", where, s.Options)
	}
	if dial.Detour != "" && !outboundTags[dial.Detour] {
		return fmt.Errorf("%s: detour %q is not an outbound tag", where, dial.Detour)
	}
	if dr := dial.DomainResolver; dr != nil && dr.Server != "" && !dnsTags[dr.Server] {
		return fmt.Errorf("%s: domain_resolver %q is not a dns server tag", where, dr.Server)
	}
	return nil
}

// validateTLS allows only options that carry no file path and no client identity.
func validateTLS(where string, tls *option.OutboundTLSOptions) error {
	if tls == nil {
		return nil
	}
	return denyKeys(where+" tls", tls,
		"enabled", "disable_sni", "server_name", "insecure", "alpn", "min_version", "max_version",
		"cipher_suites", "curve_preferences", "certificate", "certificate_public_key_sha256",
		"fragment", "fragment_fallback_delay", "record_fragment")
}

func validateDNSRules(rules []option.DNSRule, dnsTags map[string]bool) error {
	for i := range rules {
		r := &rules[i]
		var action option.DNSRuleAction
		switch r.Type {
		case C.RuleTypeDefault:
			d := &r.DefaultOptions
			if nonEmpty(d.RuleSet) {
				return errors.New(`dns rule: "rule_set" is not allowed`)
			}
			if nonEmpty(d.Geosite) || nonEmpty(d.GeoIP) || nonEmpty(d.SourceGeoIP) {
				return errors.New(`dns rule: geosite/geoip matching is not allowed`)
			}
			action = d.DNSRuleAction
		case C.RuleTypeLogical:
			if err := validateDNSRules(r.LogicalOptions.Rules, dnsTags); err != nil {
				return err
			}
			action = r.LogicalOptions.DNSRuleAction
		default:
			return fmt.Errorf("dns rule: unknown rule type %q", r.Type)
		}
		switch action.Action {
		case C.RuleActionTypeRoute:
			if !dnsTags[action.RouteOptions.Server] {
				return fmt.Errorf("dns rule: server %q is not a dns server tag", action.RouteOptions.Server)
			}
		case C.RuleActionTypeRouteOptions, C.RuleActionTypeReject, C.RuleActionTypePredefined:
		default:
			return fmt.Errorf("dns rule: action %q is not allowed", action.Action)
		}
	}
	return nil
}

// ---- route --------------------------------------------------------------------

func validateRoute(route *option.RouteOptions, outboundTags, dnsTags map[string]bool) error {
	if route == nil {
		return errors.New(`route: "final" is required`)
	}
	// rule_set/geoip/geosite (file paths, downloads), default_interface/default_mark,
	// override_android_vpn and default_network_* are denied by omission.
	if err := denyKeys("route", route, "rules", "final", "find_process", "auto_detect_interface", "default_domain_resolver"); err != nil {
		return err
	}
	if !outboundTags[route.Final] {
		return fmt.Errorf("route.final %q is not an outbound tag", route.Final)
	}
	if dr := route.DefaultDomainResolver; dr != nil && dr.Server != "" && !dnsTags[dr.Server] {
		return fmt.Errorf("route.default_domain_resolver %q is not a dns server tag", dr.Server)
	}
	return validateRouteRules(route.Rules, outboundTags, dnsTags)
}

func validateRouteRules(rules []option.Rule, outboundTags, dnsTags map[string]bool) error {
	for i := range rules {
		r := &rules[i]
		var action option.RuleAction
		switch r.Type {
		case C.RuleTypeDefault:
			d := &r.DefaultOptions
			if nonEmpty(d.RuleSet) {
				return errors.New(`route rule: "rule_set" is not allowed`)
			}
			if nonEmpty(d.Geosite) || nonEmpty(d.GeoIP) || nonEmpty(d.SourceGeoIP) {
				return errors.New(`route rule: geosite/geoip matching is not allowed`)
			}
			action = d.RuleAction
		case C.RuleTypeLogical:
			if err := validateRouteRules(r.LogicalOptions.Rules, outboundTags, dnsTags); err != nil {
				return err
			}
			action = r.LogicalOptions.RuleAction
		default:
			return fmt.Errorf("route rule: unknown rule type %q", r.Type)
		}
		switch action.Action {
		case C.RuleActionTypeRoute:
			if !outboundTags[action.RouteOptions.Outbound] {
				return fmt.Errorf("route rule: outbound %q is not an outbound tag", action.RouteOptions.Outbound)
			}
		case C.RuleActionTypeResolve:
			if s := action.ResolveOptions.Server; s != "" && !dnsTags[s] {
				return fmt.Errorf("route rule: resolve server %q is not a dns server tag", s)
			}
		case C.RuleActionTypeRouteOptions, C.RuleActionTypeReject, C.RuleActionTypeHijackDNS, C.RuleActionTypeSniff:
		default:
			// direct/bypass carry dial fields (bind_interface, ...) and are not produced by the GUI.
			return fmt.Errorf("route rule: action %q is not allowed", action.Action)
		}
	}
	return nil
}

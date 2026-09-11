---
gsd_state_version: 1.0
milestone: v2.2
milestone_name: Update & Subscription UX
status: unknown
last_updated: "2026-09-11T03:31:19.298Z"
progress:
  total_phases: 3
  completed_phases: 3
  total_plans: 4
  completed_plans: 4
---

# Proxor — Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-03-29)

**Core value:** Proxor is fully branded, modernized, and on par with Throne — clean deployment, stable storage, complete protocols, and visual feedback when the proxy is active.
**Current focus:** Phase 20 — tray-connection-info UAT

---

## Current Position

Phase: 20 (tray-connection-info) — UAT PENDING
Plan: 1 of 1 implemented

## Milestone Progress

**Milestone:** v2.2 Update & Subscription UX — In Progress

| Phase | Name | Status |
|-------|------|--------|
| 18 | Update UX | UAT deferred |
| 19 | Subscription Status UX | UAT deferred |
| 20 | Tray Connection Info | UAT pending |

Implementation: ██████████ 100% (Windows UAT outstanding)

---

## Blockers/Concerns

Local GUI compilation is unavailable: `build/` targets Windows/NMake and bundled Qt contains Windows-only tools. Phases 18-20 require Windows UAT.

Linux delivery task Phase 1 passed GitHub Actions Debian 12 AppImage runtime smoke in run `34535100324`; desktop AppImage UAT also passed TUN startup and SSH/private-route reachability. Resume behavior remains pending UAT.

Historical Phase 02 execution resumed 2026-09-11: Plans 02-01, 02-03, 02-04, and 02-12 are complete locally. Plan 02-12 now pins package workflow action provenance; Windows CI produces portable and winget candidate assets without creating a release or catalog entry. Plan 02-02 integration is present but intentionally owner-held in the dirty worktree; do not stage, reset, revert, or otherwise alter its production files. Plans 02-05 through 02-11 remain for Debian, RPM, AUR, Flatpak, release fan-in, and documentation infrastructure. Windows UAT for phases 18-20 is explicitly deferred by user instruction.

---
- Phase 02 package follow-up: Flatpak Plans 02-08/09, Arch CI/.SRCINFO (02-07), release publisher (02-10), and protected winget validation (02-11) remain incomplete; Docker and format CLIs require CI validation.

### Quick Tasks Completed

| # | Description | Date | Commit | Directory |
|---|-------------|------|--------|-----------|
| 260323-001 | commit review prerelease | 2026-03-23 | — | [260323-001](./quick/260323-001-commit-review-prerelease/) |
| 260323-17n | geosite db not found — not being added | 2026-03-23 | — | [260323-17n](./quick/260323-17n-geosite-db-not-found-no-se-esta-agregand/) |
| 260324-4et | Fix UDP packet connections closing at ~100ms | 2026-03-24 | — | [260324-4et](./quick/260324-4et-fix-udp-packet-connections-closing-at-10/) |
| 260324-4tq | Review and fix app default settings | 2026-03-24 | — | [260324-4tq](./quick/260324-4tq-review-and-fix-app-default-settings-espe/) |
| 260324-6tq | Fix test URL DNS direct empty outbound | 2026-03-24 | — | [260324-6tq](./quick/260324-6tq-fix-test-url-dns-direct-empty-outbound/) |
| 260324-sw6 | Fix Tailscale traffic routed through proxy | 2026-03-24 | — | [260324-sw6](./quick/260324-sw6-fix-tailscale-traffic-routed-through-pro/) |
| 260324-tgw | Fix high perceived connection latency | 2026-03-24 | — | [260324-tgw](./quick/260324-tgw-fix-high-perceived-connection-latency-in/) |
| 260324-tsw | Fix profile list to single-select deselect | 2026-03-24 | — | [260324-tsw](./quick/260324-tsw-fix-profile-list-to-single-select-desele/) |
| 260324-u2l | Fix missing IPv4 fakeip address range DNS | 2026-03-24 | — | [260324-u2l](./quick/260324-u2l-fix-missing-ipv4-fakeip-address-range-dn/) |
| 260325-thk | La fase solo le puso fondo dark a los logs | 2026-03-25 | — | [260325-thk](./quick/260325-thk-la-fase-solo-le-puso-fondo-dark-a-los-lo/) |
| 260329-0nc | Si la tab es de sub, agrega una col de quota | 2026-03-29 | — | [260329-0nc](./quick/260329-0nc-si-la-tab-es-de-sub-agrega-una-col-de-qu/) |
| 260329-12w | Release v1.4.0 — bump version, changelog update | 2026-03-29 | 3274141 | [260329-12w](./quick/260329-12w-release-v1-4-0-bump-version-changelog-up/) |
| 260329-pgo | Fix URL test RTT timing (WroteRequest) + typo fix | 2026-03-29 | 69b34b2 | [260329-pgo](./quick/260329-pgo-el-url-test-debe-estar-mal-planteado-sie/) |
| 260329-rj7 | Remove forced update version, commit all, and generate release changelog | 2026-03-29 | — | [260329-rj7-remove-forced-update-version-commit-all-](./quick/260329-rj7-remove-forced-update-version-commit-all-/) |
| 260330-3os | Fix xudp packet_encoding not applied to vless:// and vmess:// link parsers | 2026-03-30 | 26579f6 | [260330-3os](./quick/260330-3os-fix-no-aplica-el-xudp-del-los-links/) |
| 260330-3v2 | Redo last commit without co-author, update v1.4.1 release changelog | 2026-03-30 | 184e315 | [260330-3v2](./quick/260330-3v2-redo-last-commit-without-coauth-update-r/) |
| 260330-42y | Move v1.4.1 tag to latest commit 4bd5776 | 2026-03-30 | 4bd5776 | [260330-42y](./quick/260330-42y-move-v1-4-1-tag-and-release-source-code-/) |
| 260331-uvq | Skip startup update check when offline, retry reactively via QNetworkInformation | 2026-03-31 | 50d08e7 | [260331-uvq](./quick/260331-uvq-si-no-hay-inet-el-updater-bloquea-el-ini/) |
| 260331-vag | Release v1.4.2 — bump version, update changelog | 2026-03-31 | 513ce36 | [260331-vag](./quick/260331-vag-release-v1-4-2-bump-version-update-chang/) |
| 260416-pnm | Fix UI bugs: arrows still appearing and theme list incomplete with wrong names | 2026-04-16 | 352f847 | [260416-pnm](./quick/260416-pnm-fix-ui-bugs-arrows-still-appearing-and-t/) |
| 260416-ptk | Fix sub_update_on_start to defer until network online | 2026-04-16 | 44bcff3 | [260416-ptk](./quick/260416-ptk-fix-sub-update-on-start-to-defer-until-n/) |
| 260416-qol | Fix subscription request headers to match server API spec | 2026-04-16 | dbe4efb | [260416-qol](./quick/260416-qol-fix-subscription-request-headers-to-matc/) |
| 260610-hyk | com esta ubicado el quota? debe estar centrado entre las dos tablas de manera vertical | 2026-06-10 | 13b40b1 | [260610-hyk](./quick/260610-hyk-com-esta-ubicado-el-quota-debe-estar-cen/) |
| 260610-hvx | los botones de arriba, tienen com un borde recto abajo | 2026-06-10 | 2debf1c | [260610-hvx](./quick/260610-hvx-los-botones-de-arriba-tienen-com-un-bord/) |
| 260610-s6s | Fix profiles table column-width behavior (default/min/max, manual-mode activation) | 2026-06-10 | a60cb38 | [260610-s6s](./quick/260610-s6s-arregla-la-tabla-de-perfiles-comportamie/) |
| 260614-pg1 | Configurable ping (TCP/ICMP/HEAD), ICMP+silent subscription auto-ping, TUN "address already exists" start workaround | 2026-06-14 | — | [260614-pg1](./quick/260614-pg1-configurable-ping-tcp-icmp-head-and-tun/) |
| 260819-ryl | Fix startup crash: null `defaultClient` dereferenced by CheckUpdate, racing setup_grpc() on the DS_cores thread | 2026-08-19 | 5f30f53 | [260819-ryl](./quick/260819-ryl-fix-crash-al-abrir-defaultclient-null-en/) |
| 260829-vjy | Silence the spurious `-1919` gRPC error logged by the startup update check before the core is up | 2026-08-29 | 0739968 | [260829-vjy](./quick/260829-vjy-silenciar-el-error-1919-en-el-chequeo-de/) |
| 260909-pdt | Fix startup network trigger consumed by an unrelated reachability change (SingleShotConnection) | 2026-09-09 | 69fe038 | [260909-pdt](./quick/260909-pdt-arreglar-el-trigger-de-red-que-se-consum/) |
| 260909-pgl | Tolerant changelog markdown parser + `docs/Release_Notes_Format.md` authoring standard | 2026-09-09 | 74fa8aa, 3c410ef | [260909-pgl](./quick/260909-pgl-hacer-tolerante-el-parser-de-markdown-de/) |
| 260909-pl6 | Archive PDBs per release so crashes from shipped builds stay diagnosable | 2026-09-09 | 8185784 | [260909-pl6](./quick/260909-pl6-archivar-simbolos-pdb-por-release-para-p/) |
| 260909-pny | Log to disk with levels; crash dumps get a companion log of preceding lines | 2026-09-09 | 8b9cff8 | [260909-pny](./quick/260909-pny-log-a-disco-con-niveles-y-adjuntar-conte/) |
| 260909-lnx | Linux runtime compatibility: geodata, Tun guidance, taskbar icon, and system dark mode | 2026-09-09 | — | [260909-lnx](./quick/260909-lnx-linux-runtime-compatibility/) |
| 260910-tun | TUN startup ordering, private-route bypass, and resume subscription checks | 2026-09-10 | aa05d6d3, a9b3660b, ab5014f9, d8fd9f2c | [260910-tun](./quick/260910-tun-startup-routing-and-resume/) |

---

## Accumulated Context

- sing-box runs as an unmodified upstream submodule (Phase 12)
- Log colors fixed for light background; log and connections have live text filters (Phase 14)
- Tray single-instance and right-click subscription complete (Phase 15)
- WiFi SSID on-demand auto-connect complete (Phase 16)
- v1.4.0 released 2026-03-29
- v2.2 scope: update progress dialog (UPD-01/02/03), subscription inline status + timestamp (SUB-01/02), tray connection duration (TRAY-03)
- Linux compatibility TUN waits for a running profile, bypasses local and overlay routes, and defers automatic subscription checks until a resumed network is usable.
- Release tags and version strings MUST be X.Y.Z (three components). Two-component tags like `1.6` break auto-update on clients older than 1.6 — `parseReleaseVersion` requires `\d+\.\d+\.\d+`

### Roadmap Evolution

- Phase 46 added: Hybrid SSID on-demand with explicit profile selection, auto-connect only on matching SSIDs, manual preservation on non-matching SSIDs, and sing-box-aware active-session support

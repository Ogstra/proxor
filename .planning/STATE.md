---
gsd_state_version: 1.0
milestone: v2.2
milestone_name: Update & Subscription UX
status: unknown
last_updated: "2026-09-25T00:00:00.000Z"
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
**Current focus:** Phase 47 — Linux Update Channels complete; 47-04's AppImage self-update UAT deferred to the v1.6.9 cycle

---

## Current Position

Phase: 47 (linux-update-channels) — implementation complete, UAT deferred
Plan: 5 of 5 implemented (47-01, 47-02, 47-03, 47-05 fully complete; 47-04 implemented, Task 3 human-verify deferred, not approved)

## Milestone Progress

**Milestone:** v2.2 Update & Subscription UX — In Progress

| Phase | Name | Status |
|-------|------|--------|
| 18 | Update UX | UAT deferred |
| 19 | Subscription Status UX | UAT deferred |
| 20 | Tray Connection Info | UAT pending |

Implementation: ██████████ 100% (Windows UAT outstanding)

## Phase 47 Status (Linux Update Channels — standalone track, not part of v2.2)

| Phase | Name | Status |
|-------|------|--------|
| 47 | Linux Update Channels | UAT deferred |

Implementation: ██████████ 100% (shipped in v1.6.8; 47-04's real AppImage replace-and-relaunch cycle outstanding — see Blockers/Concerns)

---

## Blockers/Concerns

Phase 47 (Linux Update Channels) shipped in v1.6.8 (2026-09-12, CI green at commit `5da27a15`). What shipped is verified only by CI plus the deb/rpm container smoke tests — channel detection, dialog wiring, `DecideUpdaterLaunch`/`DecideAppImageApply` unit tests, and SHA256SUMS checksum verification. The AppImage self-update path (plan 47-04, LUC-04) has no runtime verification yet: no real `.AppImage` has downloaded a new version of itself, replaced its own file, and relaunched into it. Verifying that needs two releases that both contain this code, and only v1.6.8 does so far — the cycle becomes testable once v1.6.9 exists, since v1.6.8's AppImage will be the first build able to replace itself. The owner deferred plan 47-04's `checkpoint:human-verify` (Task 3) rather than block the phase on it; the seven verification steps are preserved in `.planning/phases/47-linux-update-channels/47-04-SUMMARY.md` for whoever runs them once v1.6.9 exists.

Local GUI compilation is unavailable: `build/` targets Windows/NMake and bundled Qt contains Windows-only tools. Phases 18-20 require Windows UAT.

Linux delivery task Phase 1 passed GitHub Actions Debian 12 AppImage runtime smoke in run `34535100324`; desktop AppImage UAT also passed TUN startup and SSH/private-route reachability. Resume behavior remains pending UAT.

Historical Phase 02 execution resumed 2026-09-11: Plans 02-01, 02-03 through 02-08, 02-11, and 02-12 are complete locally. Plan 02-12 pins package workflow action provenance; Windows CI produces portable and winget candidate assets without creating a release or catalog entry. Plan 02-02 integration is present but intentionally owner-held in the dirty worktree; do not stage, reset, revert, or otherwise alter its production files. Plans 02-09 and 02-10 remain gated on a real Flatpak bundle CI run. Windows UAT for phases 18-20 is explicitly deferred by user instruction.

---
- Phase 02 follow-up 2026-09-11: source closure, Arch fixture CI, source-first release guard, and protected winget validation are implemented. Flatpak release remains blocked until CI runs the real restricted bundle, offline rebuild, install, and launch; no `.flatpak` asset or final release is allowed before that gate.

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
- Phase 47 added: Linux update channels -- channel detection from package markers, package-managed installs routed to their own package manager, no exit reason the app cannot honour, AppImage self-update against $APPIMAGE, and downloads verified against the release SHA256SUMS
- Phase 47 Plan 01 complete (2026-09-12, commits a224d828/1ccb7607/47cde1b8): landed the owner's CurrentPackageMode/DecidePackageUpdate wiring as its own commit (separated from a repo-wide CRLF line-ending flip in the working tree), added DecideFlatpakLifecycle refusal at the Tun/System Proxy toggle points, added a pure DecideUpdaterLaunch policy gating every path that sets exit_reason=1, and wired test-policy-wiring.sh + test-stage-native-root.sh into the package-policy-tests CI job. LUC-03 marked complete. Local Qt6 is not installed on this machine, so run-tests.sh itself must still be proven green in CI on both runners. Plans 47-02..47-05 remain.
- Phase 47 Plan 02 complete (2026-09-13, commits d86453ea/241bcf38/b26d49f9/1e511677): PackageMode grew to deb/rpm/arch/AppImage plus a fail-closed NativeUnknownManager, detected only from a marker file or an environment variable; DecidePackageUpdate became a per-mode table with allowCheck true on every channel (including Winget/Flatpak, which previously skipped the check entirely); CurrentPackageMode() now resolves and caches the real marker/env inputs on Linux and main.cpp logs the detected channel at startup; DialogUpdateAvailable gained a guidance row (command field + non-closing Copy button, or a sentence) asserted by a new headless CTest target under QT_QPA_PLATFORM=offscreen. LUC-01/LUC-02/LUC-06 marked complete. A stray `git add` briefly swept the owner's uncommitted connectionElapsedTimer line into a commit; fixed with a follow-up commit that removed it from history and restored it uncommitted, verified clean by git diff. Plans 47-03..47-05 remain; run-tests.sh itself (all three CTest targets, including the new dialog widget test) still needs its first green run in CI, since Qt6 is not installed locally.
- Phase 47 Plan 03 complete (2026-09-13, commits 1fd66da7/e44f6875/dc5dec1c): removed the `809d48ec` stopgap that errored Check for all of Linux; added UpdateReq.channel (explicit on the wire, never inherited from the GUI's environment) and suffixesForChannel, which resolves the asset suffix each channel's release actually publishes (deb/rpm/flatpak/winget/arch/appimage/portable), falling back to GOOS/GOARCH for an empty or unrecognised channel; added UpdateReq.download_dir so the AppImage channel can land its download beside $APPIMAGE instead of inside its own read-only FUSE mount; Download now writes through a .part file and only renames into the final name once checksumForAsset/verifyAssetChecksum confirm the streamed SHA-256 matches the release's SHA256SUMS entry by base name, failing closed (removing the .part, reporting an error) on no SHA256SUMS published, no matching line, an ambiguous duplicate, or a mismatch. LUC-05/LUC-07 marked complete. protoc-gen-go/protoc-gen-go-grpc were not installed locally and were installed via `go install` at update_proto.sh's pinned versions. Plans 47-04/47-05 remain; the C++ side (set_channel/set_download_dir naming-convention risk, full mainwindow_grpc.cpp compile) is unverified locally and awaits CI (build-cpp, package-policy-tests), as is the real Check-against-live-GitHub-API and real-download UAT this plan's own verification section defers.
- Phase 47 Plan 05 complete (2026-09-12, commits fc92d3d2/17f89776/9c2e416f): `stage-native-root.sh` now requires a `--channel <deb|rpm|arch>` argument, validated before any staging happens, and writes `usr/share/proxor/package-channel` (mode 0644) -- the marker plan 47-02's `CurrentPackageMode()` already reads. All three native recipes (`debian/rules`, `proxor.spec`, `PKGBUILD.in`) pass their own channel; the rpm spec's exhaustive `%files` gained the matching entry. All four package-side tests (`test-stage-native-root.sh` plus the deb/rpm/arch integration tests) assert the marker positively; the deb and rpm tests additionally grep the installed app's startup log for `Install channel: deb`/`rpm` after the Xvfb smoke launch, proving the marker end to end. LUC-01 confirmed complete. `test-stage-native-root.sh` ran and passed locally; the deb/rpm/arch container tests themselves (Docker unavailable on this host) were checked with `bash -n` and careful review only -- their first real run is the `workflow_dispatch` release pipeline. Plan 47-04 remains.
- Phase 47 Plan 04 implemented (2026-09-13, commits 082be287/1957c3fb/557b957c): `DecideAppImageApply` decides replace-or-keep as a pure function (path-known, staged-file-exists, dir-writable, file-writable, in that order, each with a pinned refusal reason); `onUpdateStaged()` branches on `PackageMode::AppImage` before the `DecideUpdaterLaunch`/`./updater` gate from plan 47-01, replaces `$APPIMAGE` with `std::rename` (not `QFile::rename`, which refuses an existing destination) after a `DecideAppImageApply` check, and relaunches through the existing `exit_reason = 2` path -- no fourth exit reason. `on_menu_exit_triggered()` now prefers `$APPIMAGE` over `QApplication::applicationFilePath()` on Linux, which also fixes the pre-existing "Restart Program" and Tun-admin-restart paths relaunching from the AppImage's dying per-run FUSE mount. Shipped in v1.6.8 (2026-09-12, commit `0134c1c1`; CI green at `5da27a15` after two unrelated container-test fixes). Task 3 (`checkpoint:human-verify`, the real replace-and-relaunch cycle) was **deferred, not approved**, by owner decision on 2026-09-25: verifying a self-update needs two releases that both contain this code, and only v1.6.8 exists so far, so the cycle becomes testable once v1.6.9 exists. LUC-04 remains unchecked in REQUIREMENTS.md pending that UAT. Phase 47 is otherwise complete -- all 5 plans implemented, LUC-01/02/03/05/06/07 confirmed complete, only LUC-04's runtime verification outstanding.

# Roadmap: Proxor

## Milestones

- ✅ **v2.0 Throne Port & Modernization** — Phases 1–11 (shipped 2026-03-23)
- ✅ **v2.1 UX Refinements & Upstream Purity** — Phases 12–16 (shipped 2026-03-29)
- 📋 **v2.2 Update & Subscription UX** — Phases 18–20 (in progress)
- 📋 **v2.3 RAM & Memory Efficiency** — Phases 21–25 (planned)

## Phases

<details>
<summary>✅ v2.0 Throne Port & Modernization (Phases 1–11) — SHIPPED 2026-03-23</summary>

- [x] Phase 01: C & CMake Branding
- [x] Phase 02: Go Updater Branding
- [x] Phase 03: Icon Assets
- [x] Phase 04: Deploy Cleanup
- [x] Phase 05: Tray UI Polish
- [x] Phase 06: Win11 Dark Mode
- [x] Phase 07: SQLite Storage
- [x] Phase 10: sing-box Upgrade and Bug Fixes
- [x] Phase 11: Repository Structure Reorganization

See: `.planning/milestones/v2.0-ROADMAP.md` *(not yet archived — predates GSD tracking)*

</details>

<details>
<summary>✅ v2.1 UX Refinements & Upstream Purity (Phases 12–16) — SHIPPED 2026-03-29</summary>

- [x] Phase 12: sing-box Pure Submodule — unmodified upstream submodule via `3rdparty/sing-box`
- [x] Phase 14: Log & Connections UX — light-bg readability, real-time filter, sortable columns
- [x] Phase 15: Tray & Window Behavior — single-instance raise, right-click subscription add
- [x] Phase 16: On-Demand WiFi SSID — auto-start/stop proxy based on connected SSID
- [~~Phase 17~~]: Win10/Win11 Visual Harmony — *dropped, deferred*

See: [`.planning/milestones/v2.1-ROADMAP.md`](.planning/milestones/v2.1-ROADMAP.md)

</details>

### 📋 v2.2 Update & Subscription UX (Phases 18–20)

- [ ] **Phase 18: Update UX** — Non-blocking progress dialog; silent install on close (implemented, UAT deferred)
- [ ] **Phase 19: Subscription Status UX** — Inline per-group status indicator and last-updated timestamp (implemented, UAT deferred)
- [ ] **Phase 20: Tray Connection Info** — Active profile name and connection duration in tray tooltip (implemented, UAT pending)

### Phase 18: Update UX
**Goal**: Users can monitor update downloads without losing access to the app, and downloaded updates install silently on next close
**Depends on**: Nothing (new subsystem)
**Requirements**: UPD-01, UPD-02, UPD-03
**Success Criteria** (what must be TRUE):
  1. When an update is available and the user initiates download, a progress dialog appears showing a progress bar, current download speed, bytes downloaded / total size, and estimated time remaining
  2. The main window and profile list remain fully interactive while the download is in progress — the user can switch profiles, open settings, or use the tray
  3. After the download finishes the dialog closes (or collapses to a status indicator) with no "Restart now?" prompt; a tray notification or status badge confirms the update is staged
  4. On the next app close after a staged update, the installer runs silently without any additional user prompts
**Plans**: 2 executed

### Phase 19: Subscription Status UX
**Goal**: Users can see at a glance whether each subscription group is updating, succeeded, or failed — and when it was last refreshed — without opening any dialog
**Depends on**: Nothing (UI-only enhancement to existing subscription rows)
**Requirements**: SUB-01, SUB-02
**Success Criteria** (what must be TRUE):
  1. While a subscription group is being refreshed, its row shows a spinner in place of the static icon or status cell
  2. After a successful refresh the spinner is replaced by a ✓ indicator; after a failure it shows a ✗ indicator
  3. Each subscription group row displays the last-updated timestamp (e.g., "2h ago" or a full datetime) visible without any hover or expansion
**Plans**: 1 implemented, UAT deferred

### Phase 20: Tray Connection Info
**Goal**: Users can read how long the current connection has been active directly from the tray tooltip without opening the main window
**Depends on**: Nothing (tray tooltip enhancement)
**Requirements**: TRAY-03
**Success Criteria** (what must be TRUE):
  1. When a profile is active, hovering the tray icon shows a tooltip that includes the active profile name and the elapsed connection duration (e.g., "MyProfile • 2h 15m")
  2. The duration updates in real time — each time the user re-hovers, the elapsed time reflects the current value
  3. When no profile is active, the tooltip shows the previous baseline text (app name / "Not connected") with no duration field
**Plans**: 1 implemented, UAT pending

### 📋 v2.3 RAM & Memory Efficiency (Phases 21–25)

- [ ] **Phase 21: Low-Risk Churn Reduction** — Reuse long-lived helpers, stop needless full UI rebuilds, batch background refresh
- [ ] **Phase 22: Group/Profile Indexing** — Replace full-catalog scans with maintained membership indices
- [ ] **Phase 23: Metadata Shell and Explicit Hydration** — Keep hot display metadata resident and make heavy bean-dependent paths explicit
- [ ] **Phase 24: Main Proxy List Virtualization** — Migrate main proxy list to model/view for lower per-row memory cost
- [ ] **Phase 25: Secondary View Virtualization & Memory Polish** — Finish remaining high-count widget surfaces and allocation hotspots

### 📋 Future Phases

- [ ] **Phase 02: Cross-Platform Package Distribution** — Windows portable ZIP + winget; Debian/Ubuntu `.deb`, Fedora RPM, source AUR, and restricted Flatpak (standalone delivery track)
- [ ] **Phase 46: Hybrid SSID On-Demand** — Auto-connect on matching SSID, preserve manual control on non-matching SSIDs, and use sing-box Wi-Fi awareness only where it fits

---

## Phase Details

### Phase 02: Cross-Platform Package Distribution
**Goal**: Users can install Proxor through supported Windows and Linux channels with one clear update owner, verified native artifacts, and no unsafe Flatpak host privileges
**Depends on**: Existing Windows portable release and Linux AppImage path (both preserved)
**Requirements**: DIST-01, DIST-02, DIST-03, DIST-04, DIST-05, DIST-06, DIST-07
**Success Criteria** (what must be TRUE):
  1. Direct Windows portable ZIP remains self-updating while winget installs use the marker-bearing portable ZIP and `winget upgrade`
  2. Debian/Ubuntu `.deb`, Fedora RPM, source AUR recipe, and Flatpak derive from an immutable recursive source archive and pass their clean install/build checks
  3. Flatpak resolves its geodata from `/app/share/proxor` and exposes neither TUN nor host system-proxy capability
  4. AppImage remains built and Debian 12 smoke-tested; macOS and MSI remain out of scope
  5. One verified GitHub Release publishes assets/checksums; winget/AUR/Flathub/Fedora catalog steps are protected manual handoffs
**Plans**: 4 complete; 02-02 integration intentionally remains owner-held in the dirty worktree; 02-05 through 02-11 remain.

Plans:
- [x] 02-01-PLAN.md — Minimal package-mode test contract
- [ ] 02-02-PLAN.md — Managed update, Flatpak capability, and asset lookup gates (present, intentionally uncommitted/owner-held)
- [x] 02-03-PLAN.md — Shared native FHS staging contract
- [x] 02-04-PLAN.md — Windows portable/winget packages and CI validation
- [x] 02-05-PLAN.md — Debian native package and acceptance CI
- [x] 02-06-PLAN.md — Fedora native RPM and acceptance CI
- [x] 02-07-PLAN.md — Source AUR recipe and non-root CI
- [x] 02-08-PLAN.md — Flatpak source closure and metadata
- [ ] 02-09-PLAN.md — Restricted Flatpak bundle and CI (restricted bundle runtime gate pending)
- [ ] 02-10-PLAN.md — Release fan-in and checksums (blocked on verified Flatpak bundle)
- [x] 02-11-PLAN.md — Documentation and protected post-release handoffs
- [x] 02-12-PLAN.md — Immutable recursive source/toolchain provenance contract

### Phase 21: Low-Risk Churn Reduction
**Goal**: Reduce avoidable allocation churn in hot paths before deeper architecture changes
**Depends on**: Nothing
**Requirements**: MEM-01, MEM-02, MEM-03, MEM-08, MEM-09
**Success Criteria** (what must be TRUE):
  1. Background refresh paths no longer rebuild whole UI surfaces when only labels/status changed
  2. Traffic/statistics refresh no longer triggers unnecessary per-row UI work when data is unchanged or not visible
  3. Subscription/network helper objects that can be safely reused are no longer recreated on every request
  4. No behavior changes are visible to the user beyond lower resource usage
**Plans**: TBD

### Phase 22: Group/Profile Indexing
**Goal**: Make group membership lookup memory- and CPU-efficient for large catalogs
**Depends on**: Phase 21
**Requirements**: MEM-01, MEM-04, MEM-08, MEM-09
**Success Criteria** (what must be TRUE):
  1. Group profile lookup no longer scans the entire profile map in hot paths
  2. Add/move/delete/subscription-refresh workflows keep indices correct
  3. Large groups switch faster and produce fewer temporary allocations
**Plans**: TBD

### Phase 23: Metadata Shell and Explicit Hydration
**Goal**: Introduce summary-shell metadata and explicit hydration boundaries so hot UI/list paths no longer depend on direct bean access everywhere
**Depends on**: Phase 22
**Requirements**: MEM-01, MEM-05, MEM-08, MEM-09
**Success Criteria** (what must be TRUE):
  1. Summary metadata remains instantly accessible for hot UI/list/sort/render paths
  2. Edit/export/connect/test flows still work correctly through explicit hydration-required paths
  3. The codebase is ready for Phase 24 list virtualization without attempting full bean-cold persistence yet
**Plans**: TBD

### Phase 24: Main Proxy List Virtualization
**Goal**: Replace the main item-based table with a model/view path that scales to large groups
**Depends on**: Phase 23
**Requirements**: MEM-01, MEM-06, MEM-08, MEM-09
**Success Criteria** (what must be TRUE):
  1. The main proxy list no longer allocates per-cell `QTableWidgetItem` state for the entire visible dataset
  2. Sorting, filtering, selection, drag/drop, quota display, and row refresh behavior are preserved
  3. Memory with large groups loaded is lower than after Phase 23
**Plans**: TBD

### Phase 25: Secondary View Virtualization & Memory Polish
**Goal**: Finish remaining heavy list surfaces and eliminate leftover allocation hotspots
**Depends on**: Phase 24
**Requirements**: MEM-01, MEM-07, MEM-08, MEM-09
**Success Criteria** (what must be TRUE):
  1. Secondary high-count views avoid QWidget-per-row overhead where it matters
  2. Remaining avoidable allocation hotspots identified in earlier phases are removed
  3. End-to-end regression pass confirms parity with v2.2 behavior
**Plans**: TBD

---

## Progress

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 18. Update UX | 2/2 | UAT deferred | - |
| 19. Subscription Status UX | 1/1 | UAT deferred | - |
| 20. Tray Connection Info | 1/1 | UAT pending | - |
| 21. Low-Risk Churn Reduction | 0/? | Planned | - |
| 22. Group/Profile Indexing | 0/? | Planned | - |
| 23. Lazy Entity Hydration | 0/? | Planned | - |
| 24. Main Proxy List Virtualization | 0/? | Planned | - |
| 25. Secondary View Virtualization & Memory Polish | 0/? | Planned | - |

### Phase 46: Hybrid SSID On-Demand
**Goal**: Make SSID on-demand behave like a proper hybrid supervisor: matching SSIDs auto-connect an explicit profile, non-matching SSIDs never auto-connect, manual starts stay manual, and sing-box Wi-Fi rules are used only for in-session network-aware behavior
**Depends on**: Phase 16
**Requirements**: TBD
**Success Criteria** (what must be TRUE):
  1. When the connected SSID matches a configured trigger and the proxy is not running, Proxor auto-starts the explicitly configured on-demand profile
  2. When the SSID does not match, Proxor does not auto-start anything and leaves the app in manual mode
  3. If the user manually starts a profile on a non-matching SSID, the on-demand system does not stop or override that session
  4. If a session was auto-started by SSID and the machine disconnects or moves to a non-matching SSID, only that auto-started session is stopped
  5. The on-demand settings UI stores an explicit target profile instead of depending on `remember_id` or previous manual starts
  6. Any sing-box Wi-Fi-aware rules added by this phase are limited to active-session behavior and are not relied on for launching the proxy process itself
**Plans**: 1 planned

Plans:
- [ ] 46-01 — Hybrid supervisor architecture, explicit profile selection, and sing-box-aware rule injection

---

*Last updated: 2026-04-16 — Phase 46 hybrid SSID on-demand added locally*

---

## Backlog

### Phase 999.1: DataViewHtmlGenerator — Port Throne's DataViewHtmlGenerator to proxor (BACKLOG)

**Goal:** Dedicated class that renders proxy/node details as formatted HTML for the info panel, replacing ad-hoc string concatenation. Includes theme-aware styling and structured sections for server, protocol, and routing metadata.
**Requirements:** TBD
**Plans:** 0 plans

Plans:
- [ ] TBD (promote with /gsd:review-backlog when ready)

### Phase 999.2: RawRouteEdit — Port Throne's RawRouteEdit widget to proxor (BACKLOG)

**Goal:** Raw JSON/text editor for routing rules that lets power users edit the route config directly, bypassing the structured form UI. Requires integration with the existing routing dialog and validation against sing-box route schema.
**Requirements:** TBD
**Plans:** 0 plans

Plans:
- [ ] TBD (promote with /gsd:review-backlog when ready)

### Phase 999.3: Privileged Core as a Windows Service (BACKLOG)

**Deferred 2026-08-29.** Design is complete and decisions are locked; execution is out of scope for now. Windows keeps prompting UAC on every launch, which is the accepted status quo. Full rationale, including every rejected alternative, is preserved in `.planning/phases/999.3-privileged-core-as-a-windows-service/999.3-CONTEXT.md` — do not re-derive it if this is picked up again.
**Goal**: Eliminate the per-launch UAC prompt on installed Windows deployments by moving privileged work (TUN/VPN, routing) into a Windows service that the unelevated GUI drives over an ACL'd named pipe, while portable mode keeps today's behavior
**Depends on**: -
**Requirements**: TBD
**Locked decisions** (from design discussion 2026-08-29):
  - **Single build.** One `asInvoker` binary set. Portable mode self-elevates at startup via `ShellExecute "runas"`, preserving today's prompt and behavior. No separate portable/installed artifacts.
  - **Authorization by group, not elevation.** The service accepts commands only from callers whose process token is a member of the built-in Administrators group, elevated or not. Standard users cannot drive TUN.
  - **ACL'd named pipe with a custom transport.** The pipe's security descriptor is the access control. Qt's `QNetworkAccessManager` cannot speak named pipes, so the existing TCP channel in `src/rpc/gRPC.cpp` is replaced.
  - **Two release artifacts from one compile:** portable zip plus an MSI installer, authored with WiX. Identical binaries; only packaging differs. MSI is chosen for declarative `ServiceInstall`/`ServiceControl`, `MajorUpgrade`'s stop-replace-restart sequencing, transactional rollback, and standard `msiexec /qn` silent invocation.
  - **Installed deployments take one UAC prompt at update time.** The GUI launches the installer silently. Rejected: letting the LocalSystem service apply updates, which would grant it download-and-execute capability.
**Success Criteria** (what must be TRUE):
  1. On an installed deployment with UAC enabled, launching Proxor produces no UAC prompt and the GUI runs unelevated
  2. Enabling TUN/VPN from the unelevated GUI succeeds with no UAC prompt once the service is installed
  3. The service is installed by exactly one elevated action and is fully removed on uninstall
  4. The service rejects commands from any caller whose token is not in the built-in Administrators group, regardless of elevation
  5. The GUI-to-service channel is a named pipe whose ACL denies non-Administrators, and the stdin `core_token` handshake is gone
  6. Portable mode still self-elevates at startup and never requires or contacts the service
  7. A single set of release binaries covers both modes
  8. Installs that today store config next to the exe keep working once the GUI stops running elevated
  9. The updater offers the installer to installed deployments and the zip to portable ones, and never applies a zip over an installed deployment
  10. Uninstalling removes the service completely, leaving no orphaned SCM entry
**Risks**:
  - The current auth handshake (`core_token` via the child's stdin, `ExternalProcess.cpp:181`) is structurally incompatible with a pre-started service and must be redesigned, not adapted
  - A LocalSystem service exposing a weakly-guarded local API is a local privilege-escalation vector; criterion 4 is the mitigation and must be verified, not assumed
  - Existing installs currently run fully elevated, so their config files may be administrator-owned; dropping to `asInvoker` can make them unreadable (criterion 8)
  - `updateArchiveSuffixes` (`go/grpc_server/update.go:90`) selects release assets by OS and architecture only. Left unchanged it would hand the zip to installed users, who would self-update by file replacement and desynchronise the service from the binaries
  - WiX authoring has a steep curve and `msiexec` failure logs are cryptic; budget for it
  - `proxor_core.exe` must implement an SCM control handler in Go (`golang.org/x/sys/windows/svc`); MSI registers the service but the binary must behave like one
**Plans:** 0 plans

Plans:
- [ ] TBD (promote with /gsd-review-backlog when ready)

### Phase 1: Linux runtime and cross-platform distribution

**Goal:** Validate the x86_64 release AppImage on Debian 12 while retaining the Ubuntu 22.04 compatibility build baseline
**Requirements**: Debian 12 AppImage startup smoke test
**Depends on:** None (user-prioritized Linux delivery work)
**Plans:** 1 complete, GitHub Actions validation passed (run 34471472330)

Plans:
- [x] 01-01 - Debian 12 AppImage CI smoke test and Linux release documentation

---

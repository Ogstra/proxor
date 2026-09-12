---
phase: 47-linux-update-channels
plan: 01
subsystem: update-lifecycle
tags: [qt, cpp, update, flatpak, ci, package-policy]

# Dependency graph
requires: []
provides:
  - "Owner's CurrentPackageMode/DecidePackageUpdate wiring landed as its own reviewable commit"
  - "DecideFlatpakLifecycle consulted at the Tun and System Proxy toggle entry points"
  - "Pure DecideUpdaterLaunch policy with ordered refusal reasons (absence, non-executable, unwritable)"
  - "ProxorGui::ProbeUpdaterLaunch() filling the probe from the real filesystem"
  - "exit_reason is never promoted to 1, and the updater is never spawned, without a passing DecideUpdaterLaunch check"
  - "test-policy-wiring.sh and test-stage-native-root.sh both run in CI (package-policy-tests job)"
affects: [47-02, 47-03, 47-04, 47-05]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Pure decision structs (UpdaterLaunchProbe/UpdaterLaunchDecision) separate from the Qt filesystem probe that fills them, for unit-testability"
    - "Grep-level wiring contracts folded into the same CI step as the unit tests they complement, not a separate job"

key-files:
  created: []
  modified:
    - src/main/ProxorGui.cpp
    - src/main/ProxorGui.hpp
    - src/main/PackagePolicy.hpp
    - src/main/PackagePolicy.cpp
    - src/ui/mainwindow.cpp
    - src/ui/mainwindow_grpc.cpp
    - test/package_mode/package_policy_test.cpp
    - test/package_mode/test-policy-wiring.sh
    - test/package_mode/run-tests.sh
    - .github/workflows/build-proxor-cmake.yml

key-decisions:
  - "Restored the three CRLF-churned files to HEAD and re-applied only the semantic hunks by hand, rather than committing the working tree as-is, to keep the owner's unrelated connectionElapsedTimer work out of this commit"
  - "DecideUpdaterLaunch checks existence, then executability, then writability, in that fixed order, so a missing updater always reports absence rather than unwritability"
  - "Re-check DecideUpdaterLaunch at both exit_reason promotion and the final spawn point, since the filesystem state between onUpdateStaged() and actual exit can change"
  - "Collapsed the Q_OS_WIN/non-Windows asymmetry in the updater launch to always use ProxorGui::PackageExecutablePath('updater')"

patterns-established:
  - "Flatpak lifecycle refusal at proxor_set_spmode_vpn/proxor_set_spmode_system_proxy happens before any side effect, with a MessageBoxWarning naming why, mirroring the existing proxor_set_spmode_FAILED early-return idiom"

requirements-completed: [LUC-03]

# Metrics
duration: ~70min
completed: 2026-09-12
---

# Phase 47 Plan 01: Land policy wiring and refuse unrunnable exit reasons Summary

**Separated the owner's in-progress package-policy wiring from a repo-wide CRLF line-ending flip into one clean commit, then gated every path that sets `exit_reason = 1` on a new pure `DecideUpdaterLaunch` check so the app never promises a restart into an updater binary that the native Linux packages don't ship.**

## Performance

- **Duration:** ~70 min
- **Started:** 2026-09-12T~22:30Z
- **Completed:** 2026-09-12T23:42Z
- **Tasks:** 3/3 completed
- **Files modified:** 10 (across 3 commits)

## Accomplishments

- Landed the owner's `CurrentPackageMode()` / `DecidePackageUpdate` wiring and added `DecideFlatpakLifecycle` checks at the two spots that toggle Tun and System Proxy on, without pulling in the unrelated repo-wide line-ending churn or the owner's `connectionElapsedTimer` work.
- Added a pure `DecideUpdaterLaunch(UpdaterLaunchProbe)` policy and `ProxorGui::ProbeUpdaterLaunch()`, and wired both `onUpdateStaged()` and `on_menu_exit_triggered()` in `mainwindow.cpp` to refuse (with a named reason, staying open) instead of quitting on a promise the app can't honour.
- Wired the two previously-orphaned contract scripts (`test-policy-wiring.sh`, `test-stage-native-root.sh`) into CI so they can actually fail a build.

## Task Commits

1. **Task 1: Land the owner's package-policy wiring without sweeping in their unrelated work** - `a224d828` (fix)
2. **Task 2: Refuse to set an exit reason the app cannot honour** - `1ccb7607` (fix)
3. **Task 3: Make the orphaned contract scripts run in CI** - `47cde1b8` (ci)

_Note: Task 2 was specified as TDD, but since this machine has no local Qt installation, the test additions and the implementation were written together and verified by careful manual review plus the shell-level contracts that could run locally; the single commit matches this plan's explicit commit_boundaries._

## Files Created/Modified

- `src/main/ProxorGui.hpp` / `.cpp` - `CurrentPackageMode()` declaration/definition; `ProbeUpdaterLaunch()` filling `UpdaterLaunchProbe` from `QFileInfo` on `PackageExecutablePath("updater")` and `PackageRootPath()`
- `src/main/PackagePolicy.hpp` / `.cpp` - `UpdaterLaunchProbe`, `UpdaterLaunchDecision`, and `DecideUpdaterLaunch`, pure and Qt-filesystem-free
- `src/ui/mainwindow_grpc.cpp` - `DecidePackageUpdate` gating `CheckUpdate`'s early `allowCheck` return, `allow_updater`, and the Download branch's `allowDownload` check
- `src/ui/mainwindow.cpp` - `DecideFlatpakLifecycle` refusal in `proxor_set_spmode_vpn`/`proxor_set_spmode_system_proxy`; `DecideUpdaterLaunch` consulted in `onUpdateStaged()`, the `exit_reason` promotion, and the final spawn; Windows/non-Windows updater launch collapsed to one path
- `test/package_mode/package_policy_test.cpp` - `updaterLaunchIsRefusedForEveryUnmetCondition` covering all four `DecideUpdaterLaunch` cases
- `test/package_mode/test-policy-wiring.sh` - rewritten to assert the wiring is present (previously asserted it was absent, by design, as a deliberately-failing contract); extended with `DecideUpdaterLaunch`/`ProbeUpdaterLaunch` greps
- `test/package_mode/run-tests.sh` - runs the wiring contract after `ctest`
- `.github/workflows/build-proxor-cmake.yml` - `package-policy-tests` job runs `test-stage-native-root.sh` on Linux

## Deviations from Plan

None of substance. The plan's "new edit in this task" instruction for the Flatpak lifecycle wiring was followed as written (MenuToggle/StartupRestore distinction, `MessageBoxWarning` + `proxor_set_spmode_FAILED`). Task 2's TDD sub-flow was collapsed to a single commit per the plan's own `<commit_boundaries>` (one commit per task), since no local Qt toolchain exists to run RED before GREEN — see "Deferred / Could Not Verify" below.

## Deferred / Could Not Verify

- **`test/package_mode/run-tests.sh` (the authoritative ctest + wiring-contract run):** Qt6 is not installed on this machine (`brew --prefix qt` resolves to a path that does not exist; no `Qt6Config.cmake` anywhere under `/opt/homebrew`). Per the plan's own verification section, this must be proven in CI on both `windows-latest` and `ubuntu-22.04`. Verified instead: `bash -n` syntax check, manual review of `PackagePolicy.hpp`/`.cpp` and the new test slot against the Qt type usage patterns already present in the file, and the grep-only `test-policy-wiring.sh` run directly (passes).
- **A full GUI/C++ compile of `mainwindow.cpp`/`mainwindow_grpc.cpp`:** same reason; not attempted, per the environment notes, beyond manual review of the edited regions for matching braces, existing helper signatures (`MessageBoxWarning`, `MW_show_log`, `software_name`), and correct use of `DecideUpdaterLaunch`/`DecideFlatpakLifecycle`'s actual struct shapes (cross-checked against `PackagePolicy.hpp`).

## Verifications Run

| Command | Result |
|---|---|
| `bash test/package_mode/test-policy-wiring.sh` | PASS |
| `bash -n test/package_mode/run-tests.sh test/package_mode/test-policy-wiring.sh packaging/linux/tests/test-stage-native-root.sh` | PASS |
| `ruby -e "... abort unless steps include test-stage-native-root.sh"` | PASS |
| `grep -q 'test-policy-wiring.sh' test/package_mode/run-tests.sh` | PASS |
| `bash packaging/linux/tests/test-stage-native-root.sh` | PASS |
| Task 1 full verify one-liner (commit file count ≤5, excludes owner's files, <150 changed lines, owner's GroupUpdater/connectionElapsedTimer still present, wiring script passes) | PASS |

## State of the Owner's Uncommitted Work

Confirmed still present and still uncommitted after all three commits:
- `src/ui/mainwindow.h`: `#include <QElapsedTimer>` and `QElapsedTimer connectionElapsedTimer;` member
- `src/ui/mainwindow_grpc.cpp`: `if (!connectionElapsedTimer.isValid()) connectionElapsedTimer.start();` in `proxor_start`
- `src/sub/GroupUpdater.cpp`, `src/ui/model/GroupListModel.cpp`, `src/ui/dialog_manage_groups.cpp`, `go.sum`: untouched, still dirty

A full backup (`working-tree.patch` plus per-file copies) remains at `/tmp/proxor-wip-backup-20260912-203316`, and a working-tree semantic-diff snapshot for the files this plan touched was also saved to `/tmp/p47-owner/`.

## Known Stubs

None introduced by this plan.

## Self-Check

FOUND: src/main/PackagePolicy.hpp (DecideUpdaterLaunch declared)
FOUND: src/main/PackagePolicy.cpp (DecideUpdaterLaunch defined)
FOUND: src/main/ProxorGui.cpp (ProbeUpdaterLaunch defined)
FOUND: commit a224d828
FOUND: commit 1ccb7607
FOUND: commit 47cde1b8

## Self-Check: PASSED

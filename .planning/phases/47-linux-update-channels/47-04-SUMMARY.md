---
phase: 47-linux-update-channels
plan: 04
subsystem: update-lifecycle
tags: [qt, cpp, update, appimage, linux]

# Dependency graph
requires:
  - phase: 47-01
    provides: "DecideUpdaterLaunch, the ./updater exit-reason gate this plan's AppImage branch must bypass entirely"
  - phase: 47-02
    provides: "PackageMode::AppImage, CurrentPackageMode() caching the detected channel"
  - phase: 47-03
    provides: "UpdateReq.download_dir landing the AppImage download beside $APPIMAGE; SHA256SUMS verification before the download is kept"
provides:
  - "DecideAppImageApply -- pure, filesystem-free replace-or-keep decision with ordered refusal reasons"
  - "onUpdateStaged() AppImage branch that replaces $APPIMAGE with std::rename and relaunches, never reaching the ./updater exit path"
  - "on_menu_exit_triggered() preferring $APPIMAGE over the dying FUSE-mount applicationFilePath() for exit_reason 2 and 3"
  - "docs/Run_Linux.md section naming which Linux channel updates itself and which name their own command"
affects: []

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Pure decision struct (AppImageApplyProbe/AppImageApplyDecision) separate from the Qt filesystem probe built inline in onUpdateStaged(), matching the DecideUpdaterLaunch/ProbeUpdaterLaunch split from plan 47-01"
    - "std::rename over QFile::rename for same-directory atomic replacement of a file the running process already has open, avoiding the remove-then-rename window QFile::rename's existing-destination refusal would force"

key-files:
  created:
    - .planning/phases/47-linux-update-channels/47-04-SUMMARY.md
  modified:
    - src/main/PackagePolicy.hpp
    - src/main/PackagePolicy.cpp
    - src/ui/mainwindow.h
    - src/ui/mainwindow.cpp
    - src/ui/mainwindow_grpc.cpp
    - test/package_mode/package_policy_test.cpp
    - test/package_mode/test-policy-wiring.sh
    - docs/Run_Linux.md

key-decisions:
  - "targetFileWritable is checked even though POSIX rename(2) only needs the containing directory writable, refusing to overwrite an AppImage the user cannot write to (e.g. root-owned, in a user-writable directory) rather than silently replacing a file that is not the user's to replace"
  - "The AppImage branch in onUpdateStaged() runs before the DecideUpdaterLaunch gate from plan 47-01, since the two paths are mutually exclusive and the AppImage must never reach exit_reason = 1 or ./updater"
  - "on_menu_exit_triggered()'s exit_reason == 2 || 3 branch now prefers $APPIMAGE over QApplication::applicationFilePath() on Linux, which also fixes the pre-existing 'Restart Program' menu action and the Tun admin restart relaunching from the dying per-run FUSE mount under an AppImage -- not just the new self-update path"
  - "No fourth exit_reason was introduced; the AppImage replace-then-relaunch reuses exit_reason = 2 exactly as the phase brief's locked decision specifies"

patterns-established:
  - "AppImage-specific Linux behavior in mainwindow.cpp is scoped with nested #ifdef Q_OS_LINUX inside the existing non-Windows branch, matching the Tun-compatibility-core precedent already in this file"

requirements-completed: []

# Metrics
duration: ~50min (Tasks 1-2 and docs); Task 3 (human-verify) deferred
completed: 2026-09-13 (code); UAT still outstanding as of 2026-09-25
---

# Phase 47 Plan 04: AppImage self-update -- replace and relaunch $APPIMAGE Summary

**Gave the AppImage channel a real self-update -- DecideAppImageApply decides replace-or-keep as a pure function, onUpdateStaged() replaces the file `$APPIMAGE` points at with an atomic `std::rename` and relaunches into it, and the exit path no longer relaunches from the AppImage's dying per-run FUSE mount -- but the real device-level replace-and-relaunch cycle itself remains unverified, deferred to the 1.6.9 release cycle since verifying a self-update needs two releases that both contain this code and only 1.6.8 exists so far.**

## Performance

- **Duration:** ~50 min for Tasks 1-2 and the documentation update
- **Started:** 2026-09-13T~01:20Z
- **Completed (code):** 2026-09-13T~02:10Z
- **Tasks:** 2/3 completed; Task 3 (`checkpoint:human-verify`) deferred, not approved
- **Files modified:** 8 (across 3 commits)

## Accomplishments

- `DecideAppImageApply(AppImageApplyProbe)` in `src/main/PackagePolicy.{hpp,cpp}`: a pure, filesystem-free decision checking, in fixed order, whether `$APPIMAGE` is known, the staged download exists, the target directory is writable, and the target file itself is writable -- refusing with a pinned reason string on the first unmet condition, replacing on all four true. All five cases (four refusals plus the replace case) are pinned by a new test slot.
- `MainWindow::onUpdateStaged()` now branches on `PackageMode::AppImage` before the `DecideUpdaterLaunch`/`./updater` gate from plan 47-01, so the AppImage can never take that exit path. It derives the staged path from a new `staged_asset_name` member (remembered when the `Download` RPC is issued, avoiding a second round-trip), consults `DecideAppImageApply`, and on refusal tells the user exactly where the kept download is via `MessageBoxInfo` and `MW_show_log` while leaving the app open. On approval it sets execute permissions matching the existing Tun-compatibility-core idiom, replaces `$APPIMAGE` with `std::rename` (not `QFile::rename`, which refuses an existing destination and would open a window with no working AppImage at all), and on success sets `exit_reason = 2` -- no new exit reason, `./updater` never touched.
- `on_menu_exit_triggered()`'s `exit_reason == 2 || 3` branch now prefers `qEnvironmentVariable("APPIMAGE")` over `QApplication::applicationFilePath()` on Linux when it is set, and points the child's working directory at that file's directory. This also repairs the pre-existing "Restart Program" menu action and the Tun admin restart under an AppImage, both of which previously relaunched from the per-run SquashFS mount that is torn down at exit.
- `test/package_mode/test-policy-wiring.sh` gained greps for `DecideAppImageApply` and `APPIMAGE` in `mainwindow.cpp`.
- `docs/Run_Linux.md` gained an "Update ownership per channel" section: the AppImage updates itself in place, every native package and Flatpak names its own command instead.
- This work shipped in **v1.6.8** (released 2026-09-12, commit `0134c1c1`; CI went green at `5da27a15` after two container-test fixes unrelated to this plan's code -- an Arch glob/pipefail bug and a startup-log path assumption that didn't account for the root-run test container).

## Task Commits

1. **Task 1: Decide replace-or-keep as a pure function** - `082be287` (feat)
2. **Task 2: Replace the running AppImage and relaunch it** - `1957c3fb` (feat)
3. **Documentation: update ownership per Linux channel** - `557b957c` (docs)

_Task 3 (`checkpoint:human-verify`, "Verify the real AppImage update cycle") was not executed and is not approved -- see Deferred below._

## Files Created/Modified

- `src/main/PackagePolicy.hpp` / `.cpp` -- `AppImageApplyProbe`, `AppImageApplyDecision`, `DecideAppImageApply`
- `src/ui/mainwindow.h` -- `staged_asset_name` member
- `src/ui/mainwindow.cpp` -- AppImage branch in `onUpdateStaged()`; `$APPIMAGE`-preferring relaunch in `on_menu_exit_triggered()`
- `src/ui/mainwindow_grpc.cpp` -- `staged_asset_name` set when the `Download` request is issued
- `test/package_mode/package_policy_test.cpp` -- `appImageApplyIsRefusedForEveryUnmetConditionInOrder`
- `test/package_mode/test-policy-wiring.sh` -- greps for `DecideAppImageApply` and `APPIMAGE`
- `docs/Run_Linux.md` -- "Update ownership per channel" section

## Decisions Made

See `key-decisions` in the frontmatter. No architectural deviations from the plan; the interface note in the plan's own `<interfaces>` block (preferring `$APPIMAGE` over `applicationFilePath()` in the shared `exit_reason == 2 || 3` relaunch path, rather than adding a fourth exit reason) was followed exactly as specified.

## Deviations from Plan

None of substance. Plan executed as written for Tasks 1 and 2.

## Task 3: Deferred, Not Approved

**Coordinator decision (2026-09-25):** DEFERRED. Verifying a real self-update needs two releases that both contain this code, and only one exists so far -- v1.6.8 (2026-09-12, CI green at `5da27a15`) is the first release carrying phase 47's changes, so it becomes the *source* AppImage a real update cycle can be tested against, but the cycle itself is only testable once a v1.6.9 exists to update *into*. The owner chose to defer this checkpoint rather than block the phase on it.

**What ships in v1.6.8 is verified only by CI plus the deb/rpm container smoke tests** (channel detection, dialog wiring, checksum verification, the `DecideUpdaterLaunch`/`DecideAppImageApply` unit tests). **The AppImage replace-and-relaunch path itself has no runtime verification yet** -- no real `.AppImage` has actually downloaded a new version of itself, replaced its own file, and relaunched into it, nor exercised the read-only fallback.

The seven verification steps drafted for this checkpoint, preserved verbatim for whoever runs them once v1.6.9 exists:

1. Download an older release's `proxor-X.Y.Z-linux64.AppImage` into a writable directory such as `~/Applications`, `chmod +x` it, and run it.
2. Confirm the startup log line in `~/.config/proxor/config/logs/proxor-<today>.log` reads `Install channel: appimage`.
3. Use the in-app update check. Confirm the dialog offers `Download and Restart` -- an AppImage owns its file, so it is not shown a package-manager command.
4. Accept the download. Confirm the progress dialog completes, the app closes, and it reopens reporting the new version in About.
5. Confirm the directory now holds one `.AppImage` at the original name with the new version's size, and no leftover `.part` file.
6. Repeat steps 1-4 with the AppImage made read-only (`chmod a-w`). Confirm the app stays open, names the exact path where the download was kept, and that the file is actually there.
7. If the AppImage is registered through AppImageLauncher, repeat step 4 under it. `$APPIMAGE` may point at AppImageLauncher's managed copy rather than the original download -- report what actually happened rather than assuming.

**Resume signal (unchanged):** Type "approved", or describe which step failed and what the app did instead.

## Verifications Run

| Command | Result |
|---|---|
| `bash test/package_mode/test-policy-wiring.sh` | PASS (including the two new greps) |
| Manual brace/paren balance check on `mainwindow.cpp`, `PackagePolicy.cpp`/`.hpp` | Balanced |
| `git diff --stat` vs `git diff --ignore-all-space --stat` on every touched file before each `git add` | Identical -- no CRLF churn |
| CI (`build-cpp` ubuntu-22.04/windows-latest, `package-policy-tests`) at v1.6.8 release and again at `5da27a15` | Green |

## Deferred / Could Not Verify

- **`test/package_mode/run-tests.sh` (the new `appImageApplyIsRefusedForEveryUnmetConditionInOrder` Qt Test slot, and a full compile of `mainwindow.cpp`/`mainwindow_grpc.cpp` against the new code):** no Qt6 on the local macOS host, same constraint as every prior plan in this phase. Proven green in CI as part of the v1.6.8 release and the subsequent `5da27a15` fix commit.
- **The real AppImage replace-and-relaunch cycle (Task 3, all seven steps):** deferred to the 1.6.9 cycle, per the coordinator decision above. This is the only success criterion (criterion 4 of Phase 47) with no runtime proof yet.

## State of the Owner's Uncommitted Work

Confirmed still present and unaffected by this plan's commits:
- `src/ui/mainwindow.h`: `#include <QElapsedTimer>` and `QElapsedTimer connectionElapsedTimer;` member
- `src/ui/mainwindow_grpc.cpp`: `if (!connectionElapsedTimer.isValid()) connectionElapsedTimer.start();` in `proxor_start`
- `src/sub/GroupUpdater.cpp`, `src/ui/model/GroupListModel.cpp`, `src/ui/dialog_manage_groups.cpp`, all `go.sum` files: untouched, still dirty

Both of this plan's touched files (`mainwindow.h`, `mainwindow_grpc.cpp`) had the owner's lines temporarily removed before staging, committed without them, then restored uncommitted -- verified by `git diff` showing only the owner's original lines remaining afterward.

## Known Stubs

None introduced by this plan.

## Self-Check

FOUND: src/main/PackagePolicy.hpp (DecideAppImageApply declared)
FOUND: src/main/PackagePolicy.cpp (DecideAppImageApply defined)
FOUND: src/ui/mainwindow.cpp (DecideAppImageApply / APPIMAGE)
FOUND: commit 082be287
FOUND: commit 1957c3fb
FOUND: commit 557b957c
FOUND: v1.6.8 release commit 0134c1c1 carries this plan's code
FOUND: CI green at 5da27a15 (post-release container-test fixes, unrelated to this plan's own code)

## Self-Check: PASSED

## Next Phase Readiness

Phase 47 (Linux Update Channels) is implementation-complete across all five plans (47-01, 47-02, 47-03, 47-05, and this plan's Tasks 1-2). LUC-04 remains unchecked in `.planning/REQUIREMENTS.md` pending the Task 3 UAT, consistent with how phases 18-20 leave their requirements unchecked while UAT is outstanding. No further plans are queued for this phase; the outstanding item is purely the human-verify checkpoint, re-runnable once a v1.6.9 release exists.

---
*Phase: 47-linux-update-channels*
*Completed: 2026-09-13 (code); UAT deferred as of 2026-09-25*

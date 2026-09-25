---
phase: 48-pinned-dependencies-and-supported-ci
plan: 01
subsystem: infra
tags: [ci, sha256, protobuf, zxing-cpp, yaml-cpp, flatpak, supply-chain]

# Dependency graph
requires: []
provides:
  - "fetch_verified helper (libs/build_deps_fetch.sh) hashing every native-dependency download with shasum -a 256 before use"
  - "zxing-cpp 2.0.0 and yaml-cpp 0.7.0 fetched as pinned .tar.gz archives, matching the Flatpak manifest's checksums"
  - "protobuf built from the pinned protobuf-all-21.4.tar.gz release archive instead of a git clone of a movable tag"
  - "libs/tests/test-dependency-pins.sh: offline drift guard proving build_deps_all.sh and the Flatpak manifest agree, plus a proof the mismatch path aborts and cleans up"
  - "package-policy-tests CI job runs the drift guard on both windows-latest and ubuntu-22.04"
affects: [48-02, 48-03, native-dependency-build, release-pipeline]

# Tech tracking
tech-stack:
  added: []
  patterns: ["sourceable no-side-effect shell helper pattern (fetch_verified) shared between a build script and its test"]

key-files:
  created:
    - libs/build_deps_fetch.sh
    - libs/tests/test-dependency-pins.sh
  modified:
    - libs/build_deps_all.sh
    - .github/workflows/build-proxor-cmake.yml

key-decisions:
  - "Switched zxing-cpp and yaml-cpp downloads from .zip to .tar.gz so one sha256 per archive is shared with the Flatpak manifest, instead of maintaining a second hash for a format nothing else verifies."
  - "protobuf-all-21.4.tar.gz replaces the git clone --recurse-submodules; it vendors third_party/googletest itself, so no submodule fetch is needed."
  - "Drift guard matches pins to the Flatpak manifest by URL (not position), so reordering entries can't hide a real mismatch."
  - "Drift guard runs on both matrix OSes with no Linux-only if:, since it is also the proof that shasum -a 256 exists in Git Bash on windows-latest before build_deps_all.sh starts depending on it there."

patterns-established:
  - "Downloads used by any compiled artifact go through fetch_verified; nothing calls curl directly outside that helper."

requirements-completed: [SUP-01, SUP-02, SUP-03]

# Metrics
duration: ~35min
completed: 2026-09-25
---

# Phase 48 Plan 01: Verified Dependency Downloads Summary

**Every archive libs/build_deps_all.sh downloads is now hashed against a pin shared with the Flatpak manifest before it is unpacked, and protobuf comes from the pinned protobuf-all-21.4.tar.gz release archive instead of a git clone of a movable tag.**

## Performance

- **Duration:** ~35 min
- **Started:** 2026-09-25T17:49:00Z (approx)
- **Completed:** 2026-09-25T18:24:04Z
- **Tasks:** 3
- **Files modified:** 4 (2 created, 2 modified)

## Accomplishments
- Added `fetch_verified`, a side-effect-free sourceable helper that downloads to a temp-adjacent path, hashes with `shasum -a 256`, and refuses to leave a mismatched file on disk — reporting URL, expected and actual hash on failure.
- Rewrote all three native-dependency downloads in `libs/build_deps_all.sh` (zxing-cpp, yaml-cpp, protobuf) to route through `fetch_verified` against a pin table matching `packaging/flatpak/io.github.Ogstra.Proxor.yml` byte-for-byte, with zero `git clone` and zero bare `curl` calls remaining in the script.
- protobuf now unpacks `protobuf-21.4/` from the `-all` release tarball; `CMAKE_COMPAT_ARGS` and every existing cmake flag were left untouched.
- Added `libs/tests/test-dependency-pins.sh`: proves `fetch_verified`'s success and failure paths offline via `file://` URLs (no network), proves the three pins match the Flatpak manifest by URL, and proves no `git clone`/bare `curl` remain.
- Wired that test into `package-policy-tests` in CI on both `windows-latest` and `ubuntu-22.04`, with no `Linux`-only guard, so it also proves `shasum -a 256` exists in Git Bash before the dependency build depends on it there.

## Task Commits

1. **Task 1: A fetch that refuses to hand back the wrong bytes** - `ffd71a4d` (build)
2. **Task 2: Pin all three libraries, and take protobuf off the moving tag** - `eb316764` (build)
3. **Task 3: Run the drift guard where Git Bash is, not only where bash is** - `2ec4e13e` (ci)

**Plan metadata:** pending (this commit)

## Files Created/Modified
- `libs/build_deps_fetch.sh` - New sourceable `fetch_verified(url, expected_sha256, out)` helper; no side effects at source time.
- `libs/tests/test-dependency-pins.sh` - New: offline fetch_verified proof (good/bad hash) + Flatpak-manifest drift guard + git-clone/bare-curl guards.
- `libs/build_deps_all.sh` - Sources the helper; zxing-cpp/yaml-cpp/protobuf all fetch pinned `.tar.gz` via `fetch_verified`; protobuf's `git clone` replaced by `protobuf-all-21.4.tar.gz`; `clean()` extended to remove both old (`dl.zip`, `protobuf/`) and new (`dl-*.tar.gz`, `protobuf-21.4/`) artifacts.
- `.github/workflows/build-proxor-cmake.yml` - `package-policy-tests` job gained a `Verify dependency pins against the flatpak manifest` step (no OS guard), running `libs/tests/test-dependency-pins.sh`.

## Decisions Made
- Archive format for zxing-cpp/yaml-cpp switched from `.zip` to `.tar.gz` to reuse the Flatpak manifest's existing checksums rather than inventing new ones for a format nothing else in the repo verifies (per plan's `<facts_verified_for_this_plan>`).
- Drift-guard matching is done by URL substring against the manifest's `url:`/`sha256:` pair, using only `grep`/`sed` (no `yq`/Python), since the guard must run under Git Bash on `windows-latest`.
- Left `dl.zip` and bare `protobuf` in `clean()`'s removal list so a pre-existing `libs/deps` tree from before this change is still cleaned correctly.

## Deviations from Plan

None functionally — plan executed as written. One micro-adjustment during Task 2: an explanatory comment about protobuf's vendored `third_party/` originally used the literal phrase "git clone" and tripped the test's own `grep -q 'git clone'` guard against comments as well as code; reworded the comment to "the old submodule-recursive checkout" to keep the guard's phrasing precise about actual commands rather than prose. No file changes beyond wording; not a Rule 1-4 item since it never left a bad or non-functioning state committed — caught and fixed before the Task 2 commit.

## Issues Encountered
None beyond the comment-wording note above.

## User Setup Required
None - no external service configuration required.

## Verification Run Locally
- `bash -n libs/build_deps_fetch.sh libs/build_deps_all.sh libs/tests/test-dependency-pins.sh` - clean parse, all three scripts.
- `bash libs/tests/test-dependency-pins.sh` - passed: `shasum` present, correct-hash fetch succeeds with matching bytes, wrong-hash fetch fails with both hashes in stderr and no leftover file, all three pins match the Flatpak manifest, no `git clone`, no bare `curl`.
- Task 2's full combined verify line (bash -n + test run + git-clone/curl greps + `protobuf-21.4/build` grep + cross-checking all three sha256 values appear in both `libs/build_deps_all.sh` and `packaging/flatpak/io.github.Ogstra.Proxor.yml`) - passed, printed `ALL_OK`.
- Task 3's `ruby -e` YAML-structure check (drift guard wired, not Linux-gated, matrix still covers `windows-latest`, `build-cpp`'s cache key still keys off `hashFiles('libs/build_deps_*.sh')`) - passed.

## Verification Deferred to CI
- The actual download-and-build of zxing-cpp, yaml-cpp and protobuf via the rewritten `libs/build_deps_all.sh` — this machine has no Qt6/cmake/ninja toolchain installed, so `build-cpp` has never run locally. This is the first real execution of the rewritten script; expect a slow `Build native dependencies` step since the `libs/build_deps_*.sh` cache-key hash changed and all three libraries rebuild from scratch on every matrix entry, per the plan's own note.
- `package-policy-tests` actually running green on `windows-latest` under real Git Bash (only reasoned about locally, via `bash -n` and the offline test run on macOS bash, which uses the same POSIX constructs).
- Any real archive drift detection (mismatched hash aborting `build-cpp` with the "checksum mismatch, refusing to build" message) — not exercised, since the pins were taken directly from values already verified during planning against the same URLs currently in the Flatpak manifest.

## Working Tree State
The owner's pre-existing uncommitted work is untouched and remains unstaged: `3rdparty/QHotkey`, `3rdparty/SQLiteCpp`, `3rdparty/sing-box` (submodule pointers), `3rdparty/qv2ray/v2/components/proxy/QvProxyConfigurator.cpp`, `cmake/windows/generate_product_version.cmake`, `cmake/windows/windows.cmake`, `go/cmd/proxor_core/go.mod`, `go/cmd/proxor_core/go.sum`, `go/cmd/updater/go.sum`, `go/grpc_server/go.sum`, `src/sub/GroupUpdater.cpp`, `src/sub/GroupUpdater.hpp`, `src/ui/dialog_manage_groups.cpp`, `src/ui/dialog_update_progress.cpp`, `src/ui/mainwindow.h`, `src/ui/mainwindow_grpc.cpp`, `src/ui/model/GroupListModel.cpp`, plus untracked `CMakeFiles/` and `debug/`. All four of this plan's files were staged and committed individually by path; no `git add -A`/`-A .`/`-a` was used at any point.

## Next Phase Readiness
- Plan 48-02 (CI action-pin bumps) can proceed independently; this plan touched no action pins.
- Plan 48-03, if it depends on the drift guard existing, can rely on `libs/tests/test-dependency-pins.sh` being present and wired into `package-policy-tests`.
- Outstanding: the first CI run after this merges will be slow in `build-cpp` (cache invalidation rebuilds all three native deps) — this is expected, not a regression to investigate.

---
*Phase: 48-pinned-dependencies-and-supported-ci*
*Completed: 2026-09-25*

## Self-Check: PASSED

All created/modified files confirmed present on disk (`libs/build_deps_fetch.sh`, `libs/tests/test-dependency-pins.sh`, `libs/build_deps_all.sh`, `.github/workflows/build-proxor-cmake.yml`, this SUMMARY.md). All three task commit hashes (`ffd71a4d`, `eb316764`, `2ec4e13e`) confirmed present in `git log`.

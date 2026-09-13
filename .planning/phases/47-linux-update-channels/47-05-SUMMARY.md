---
phase: 47-linux-update-channels
plan: 05
subsystem: packaging
tags: [bash, packaging, deb, rpm, arch, update-channels]

# Dependency graph
requires:
  - phase: 47-02
    provides: "Install channel: <token> logged at startup, read by the app's package-mode detection"
provides:
  - "--channel <deb|rpm|arch> required argument on stage-native-root.sh, writing usr/share/proxor/package-channel (0644)"
  - "The marker wired into all three native Linux packaging recipes, with the rpm %files entry that keeps rpmbuild working"
  - "Positive package-content and installed-app-log assertions for the marker in all four package-side tests"
affects: []

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Required rather than defaulted CLI argument in a shared staging helper, so a caller that forgets it fails loudly instead of shipping a silently-wrong package"
    - "Log-grep assertion kept separate from the launch's exit-code check, since a legitimate 124 timeout must still satisfy the startup-log assertion"

key-files:
  created: []
  modified:
    - packaging/linux/stage-native-root.sh
    - packaging/linux/tests/test-stage-native-root.sh
    - packaging/debian/debian/rules
    - packaging/rpm/proxor.spec
    - packaging/arch/PKGBUILD.in
    - packaging/debian/tests/test-deb-package.sh
    - packaging/rpm/tests/test-rpm-package.sh
    - packaging/arch/tests/test-aur-package.sh

key-decisions:
  - "Validated --channel against the exact set {deb,rpm,arch} in the same pre-staging check block as the existing --gui/--core/--geodata presence checks, so an invalid or missing channel fails before mktemp/rm -rf ever runs, preserving the helper's existing DESTDIR-untouched guarantee"
  - "Wrote the marker with printf '%s\\n' \"$channel\" followed by an explicit chmod 0644, matching the install -m 0644 semantics of every other staged file rather than relying on umask"
  - "Kept the deb/rpm log assertions as a separate grep after the existing exit-code check (0 or 124), since the startup log line is written during init and must not be conditioned on the launch's timeout outcome"
  - "Arch's test only asserts package content (bsdtar -xOf ... | grep -qx arch) with an explicit comment explaining the asymmetry with deb/rpm, since that script neither installs nor launches the package"

requirements-completed: [LUC-01]

# Metrics
duration: ~25min
completed: 2026-09-12
---

# Phase 47 Plan 05: Native package channel marker Summary

**Added a required `--channel <deb|rpm|arch>` argument to the single native-Linux staging helper that writes `usr/share/proxor/package-channel` (mode 0644), wired all three packaging recipes to pass their own channel, added the rpm `%files` entry the exhaustive spec needs to keep building, and asserted the marker's presence/content plus the installed app's startup-log proof in all four package-side tests.**

## Performance

- **Duration:** ~25 min
- **Started:** 2026-09-12
- **Completed:** 2026-09-12
- **Tasks:** 3/3 completed
- **Files modified:** 8

## Accomplishments

- `stage-native-root.sh` now requires `--channel`, validated against exactly `deb`, `rpm` and `arch` before any staging happens (mirroring the existing fail-before-mutate pattern for `--gui`/`--core`/`--geodata`), and writes `usr/share/proxor/package-channel` with mode 0644 containing the channel name followed by a newline. The staged regular-file count is now 8.
- `test-stage-native-root.sh` bumps the expected count to 8, asserts the marker's path/mode/exact content, and adds two new negative cases (missing `--channel`, invalid `--channel nonsense`) following the existing `DESTDIR`-untouched pattern.
- `packaging/debian/debian/rules`, `packaging/rpm/proxor.spec` and `packaging/arch/PKGBUILD.in` each pass their own `--channel` to the shared helper; the rpm spec's `%files` list gained the `%{_datadir}/proxor/package-channel` entry required to keep `rpmbuild` from failing with "Installed (but unpackaged) file(s) found."
- `test-deb-package.sh` and `test-rpm-package.sh` add the marker to their positive path-presence loops, assert the installed file's exact content (`deb`/`rpm`) after install, and — separately from the existing 0/124 exit-code check — grep the newest log under `$HOME/.config/proxor/config/logs` for `Install channel: deb`/`Install channel: rpm` after the Xvfb smoke launch. `test-aur-package.sh` asserts package content only (`bsdtar -xOf ... package-channel | grep -qx arch`), with a comment explaining it neither installs nor launches so it can't reach the log.

## Task Commits

1. **Task 1: Require a channel when staging a native root** - `fc92d3d2` (build)
   - Files: `packaging/linux/stage-native-root.sh`, `packaging/linux/tests/test-stage-native-root.sh`
2. **Task 2: Pass the channel from all three packaging recipes** - `17f89776` (build)
   - Files: `packaging/debian/debian/rules`, `packaging/rpm/proxor.spec`, `packaging/arch/PKGBUILD.in`
3. **Task 3: Assert the marker in the built packages and in the installed app** - `9c2e416f` (test)
   - Files: `packaging/debian/tests/test-deb-package.sh`, `packaging/rpm/tests/test-rpm-package.sh`, `packaging/arch/tests/test-aur-package.sh`

## Files Created/Modified

- `packaging/linux/stage-native-root.sh` — required `--channel` argument validated against `deb|rpm|arch`; writes `usr/share/proxor/package-channel` (0644)
- `packaging/linux/tests/test-stage-native-root.sh` — marker path/mode/content assertions, file-count bumped to 8, two new negative cases
- `packaging/debian/debian/rules` — `override_dh_auto_install` passes `--channel deb`
- `packaging/rpm/proxor.spec` — `%install` passes `--channel rpm`; `%files` gained `%{_datadir}/proxor/package-channel`
- `packaging/arch/PKGBUILD.in` — `package()` passes `--channel arch`
- `packaging/debian/tests/test-deb-package.sh` — marker in the positive path loop, post-install content check, post-launch log-grep for `Install channel: deb`
- `packaging/rpm/tests/test-rpm-package.sh` — same three additions with `rpm`
- `packaging/arch/tests/test-aur-package.sh` — package-content-only assertion via `bsdtar`, with a comment on the asymmetry

## Deviations from Plan

None. The plan's tasks, file lists and commit boundaries were followed as written.

## Verifications Run

| Command | Result |
|---|---|
| `bash -n packaging/linux/stage-native-root.sh packaging/linux/tests/test-stage-native-root.sh && bash packaging/linux/tests/test-stage-native-root.sh` | PASS (run twice, once per task-boundary check) |
| `bash -n packaging/debian/debian/rules; grep -q -- '--channel deb' ...; grep -q -- '--channel rpm' ...; grep -q -- '--channel arch' ...; grep -q '%{_datadir}/proxor/package-channel' packaging/rpm/proxor.spec` | PASS |
| `bash -n packaging/debian/tests/test-deb-package.sh packaging/rpm/tests/test-rpm-package.sh packaging/arch/tests/test-aur-package.sh` plus greps for `Install channel: deb`/`Install channel: rpm`/`package-channel` | PASS |
| `git diff --stat` vs `git diff --ignore-all-space --stat` on every touched file before each commit | Identical — no CRLF churn, only the intended hunks, confirmed before every `git add` |

## Deferred / Could Not Verify

- **The deb, rpm and Arch container-based integration tests themselves** (`test-deb-package.sh`, `test-rpm-package.sh`, `test-aur-package.sh`): Docker is not running on this host. Each script was checked with `bash -n` and reviewed line by line against the real `dpkg-deb`/`rpm -qpl`/`bsdtar` invocations already present in the file, but none of the three has actually executed against a built package in this session. **Their real run is the `workflow_dispatch` release pipeline** (`build-proxor-cmake.yml`'s `package-deb`/`package-rpm`/`package-arch` jobs and `publish-packages.yml`), per the plan's own verification section — the rpm build in particular is the first real test of the new `%files` entry, since a missing entry there is a hard `rpmbuild` failure, not a lint warning.
- **lintian/rpmlint/namcap's tolerance of the new plain 0644 data file** under `/usr/share/proxor/`: the research rated this low-but-unverified risk; the first real CI run of the three packaging jobs is what proves it, exactly as the plan's `<verification>` section states.

## State of the Owner's Uncommitted Work

Confirmed untouched after all three commits (`git diff --stat HEAD~3` on each shows only the owner's pre-existing, pre-this-plan diffs):
- `src/ui/mainwindow.h`, `src/ui/mainwindow_grpc.cpp`, `src/sub/GroupUpdater.cpp`, `src/ui/model/GroupListModel.cpp`, `src/ui/dialog_manage_groups.cpp`, `go.sum` — none staged, none edited, none referenced by any commit in this plan.
- The repo-wide CRLF-vs-LF churn noted in the task context remains present and untouched outside the eight files this plan intentionally edited; every staged diff's `--stat` matched its `--ignore-all-space --stat` exactly before commit.

## Known Stubs

None introduced by this plan. The marker is wired end to end within the scope of this plan: written by the staging helper, carried by all three recipes, declared in the rpm's `%files`, and asserted by all four package-side tests (three at package-content level, two of those also at the installed-app-log level).

## Self-Check

FOUND: packaging/linux/stage-native-root.sh (--channel argument and package-channel write)
FOUND: packaging/rpm/proxor.spec (%{_datadir}/proxor/package-channel in %files)
FOUND: packaging/debian/tests/test-deb-package.sh (Install channel: deb grep)
FOUND: packaging/rpm/tests/test-rpm-package.sh (Install channel: rpm grep)
FOUND: packaging/arch/tests/test-aur-package.sh (package-channel bsdtar assertion)
FOUND: commit fc92d3d2
FOUND: commit 17f89776
FOUND: commit 9c2e416f

## Self-Check: PASSED

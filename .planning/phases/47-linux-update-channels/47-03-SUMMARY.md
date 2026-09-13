---
phase: 47-linux-update-channels
plan: 03
subsystem: update-lifecycle
tags: [go, grpc, update, protobuf, checksum, linux]

# Dependency graph
requires:
  - phase: 47-02
    provides: "PackageMode grown to every channel, PackageModeName wire/log token, DecidePackageUpdate per-mode table"
provides:
  - "UpdateReq.channel carrying the GUI's detected PackageMode explicitly on the wire"
  - "suffixesForChannel resolving the asset suffix each Linux channel's release actually publishes, with GOOS/GOARCH fallback for an empty/unrecognised channel"
  - "UpdateReq.download_dir letting the caller choose where Download writes, validated as an existing writable directory"
  - "downloadDestination/.part write-then-rename so a half-written download is never visible under its final name"
  - "checksumForAsset/verifyAssetChecksum verifying every download against the release's SHA256SUMS by base name before it is kept"
affects: [47-04, 47-05]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Channel passed explicitly on the wire (UpdateReq.channel) rather than inherited from the GUI's environment, because the core is a child process whose environment is not an architectural contract"
    - "Streaming SHA-256 via io.MultiWriter behind the existing progressWriter, so verification never re-reads the file"
    - "fail() closure in Download: records progress error, sets response error, removes the .part file -- a single chokepoint for every failure mode"

key-files:
  created: []
  modified:
    - go/grpc_server/update.go
    - go/grpc_server/update_test.go
    - go/grpc_server/gen/libcore.proto
    - go/grpc_server/gen/libcore.pb.go
    - src/ui/mainwindow_grpc.cpp
    - test/package_mode/test-policy-wiring.sh

key-decisions:
  - "Extracted verifyAssetChecksum(sums, assetName, gotDigestHex) as a pure function so the digest-mismatch path has a real unit test -- a deliberately corrupted asset cannot be produced against a published release, so this comparison, not a live download, is what proves that path"
  - "suffixesForChannel falls back to updateArchiveSuffixes(goos, goarch) for any goos != linux, any empty channel, and any unrecognised channel, so an older GUI against a newer core keeps working"
  - "Check still resolves an asset for deb/rpm/arch/flatpak/winget even though the GUI never calls Download for them -- knowing a version exists is independent of being able to apply it, and the resolved asset name is what the C++ guidance text names"
  - "Renamed updatePackagePath to updateAssetName and moved destination resolution into the Download case itself, so a stale Check result can never be reused against a directory a different Download request names"

patterns-established:
  - "Write-then-rename through a .part suffix at the one place Download writes a file, gated on checksum verification succeeding first"

requirements-completed: [LUC-05, LUC-07]

# Metrics
duration: ~75min
completed: 2026-09-13
---

# Phase 47 Plan 03: Per-channel update assets and SHA256SUMS verification Summary

**Replaced the blanket "self-update is not available on Linux" stopgap with an explicit per-channel asset resolver driven by a new `UpdateReq.channel` field, added a caller-chosen `download_dir` for the AppImage channel's read-only-mount problem, and made every download prove itself against the release's `SHA256SUMS` before it is ever kept under its final name.**

## Performance

- **Duration:** ~75 min
- **Started:** 2026-09-12T~23:55Z
- **Completed:** 2026-09-13T~01:10Z
- **Tasks:** 3/3 completed
- **Files modified:** 6 (across 3 commits)

## Accomplishments

- `updateArchiveSuffixes("linux", "amd64")` now resolves `linux64.AppImage` instead of erroring, and the new `suffixesForChannel(channel, goos, goarch)` resolves deb/rpm/flatpak/winget/arch/appimage/portable suffixes, each verified against the real v1.6.7 release asset names (`proxor_1.6.7-1_amd64.deb`, `proxor-1.6.7-1.fc44.x86_64.rpm`, `proxor-1.6.7.flatpak`, `proxor-1.6.7-winget-x64.zip`, `proxor-1.6.7.tar.gz`, `proxor-1.6.7-linux64.AppImage`), with `parseReleaseVersion` confirmed to extract `1.6.7` from every one of them.
- `UpdateReq` grew `channel` (field 3) and `download_dir` (field 4); the GUI sets `channel` from `PackageModeName(mode)` on every `Check` and sets `download_dir` to `$APPIMAGE`'s directory only for the AppImage channel, leaving every other channel's behaviour (including Windows portable) unchanged.
- `Download` now writes to `<destination>.part`, computes the SHA-256 while streaming (`io.MultiWriter` behind the existing `progressWriter`), fetches the release's `SHA256SUMS` asset recorded at `Check` time, and only renames `.part` to the final name once `checksumForAsset` finds a matching entry by base name and the digests agree case-insensitively. Every failure mode -- no `SHA256SUMS` published, no line for the asset, an ambiguous duplicate, or a digest mismatch -- removes the `.part` file and reports an error instead of applying anything.
- The old locking test `TestLinuxUpdateGuidanceUsesPackageManagerOrAppImage` was renamed and rewritten to `TestLinuxUpdateCheckResolvesAppImageAsset`, asserting the opposite contract; the guidance-text wording it used to lock in no longer exists in this package (it lives in `src/main/PackagePolicy.cpp`, landed in 47-02).

## Task Commits

1. **Task 1: Replace the blanket Linux error with per-channel asset resolution** - `1fd66da7` (fix)
2. **Task 2: Let the caller choose where the download lands** - `e44f6875` (feat)
3. **Task 3: Verify every download against the release SHA256SUMS** - `dc5dec1c` (feat)

## Files Created/Modified

- `go/grpc_server/gen/libcore.proto` / `libcore.pb.go` - `UpdateReq.channel` (field 3) and `UpdateReq.download_dir` (field 4), regenerated with `protoc` + `protoc-gen-go`/`protoc-gen-go-grpc` (both installed locally this session; neither was present beforehand)
- `go/grpc_server/update.go` - `suffixesForChannel`, `downloadDestination`, `checksumForAsset`, `verifyAssetChecksum`; `Check` now records `updateChecksumsURL` alongside `updateDownloadURL`/`updateAssetName`; `Download` resolves its destination from `in.DownloadDir` and writes/verifies/renames through a `.part` file
- `go/grpc_server/update_test.go` - rewritten linux-resolves contract, `TestSuffixesForChannel` table test plus a real-v1.6.7-names cross-check, `downloadDestination` table tests, and `checksumForAsset`/`verifyAssetChecksum` table tests including the leading-`./` fixture, a bare-name + `*` binary marker + blank-line + trailing-`\r` fixture, base-name-only matching against a full `browser_download_url`, and the ambiguous-duplicate/mismatch/empty-sums failure paths
- `src/ui/mainwindow_grpc.cpp` - `request.set_channel(PackageModeName(mode).toStdString())` on `Check`; `request2.set_download_dir(...)` set to `$APPIMAGE`'s directory only when `mode == PackageMode::AppImage`, added `#include <QFileInfo>`
- `test/package_mode/test-policy-wiring.sh` - added a grep for `set_channel` in `mainwindow_grpc.cpp`

## Deviations from Plan

None of substance. `protoc-gen-go`/`protoc-gen-go-grpc` were not present on this machine at session start; both were installed via `go install` at the pinned versions `update_proto.sh` already specifies, which the plan's own interface notes anticipated ("protoc is on PATH on this machine").

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Regenerating the proto reshuffled an unrelated comment in `libcore_grpc.pb.go`**
- **Found during:** Task 2's `update_proto.sh` run
- **Issue:** `protoc-gen-go-grpc`'s comment-association between two RPC stubs is apparently non-deterministic across regenerations (a stray `//` line between `Update` and `Validate` in both the client and server interfaces appeared/disappeared across runs), producing a 4-line diff in a file Task 2 had no reason to touch.
- **Fix:** Restored `libcore_grpc.pb.go` to its HEAD content before staging, since the field added in Task 2 (`download_dir`) only required changes to `libcore.proto` and `libcore.pb.go`.
- **Files modified:** `go/grpc_server/gen/libcore_grpc.pb.go` (reverted to HEAD before commit; not part of the final diff)
- **Commit:** n/a (reverted before staging)

## Verifications Run

| Command | Result |
|---|---|
| `cd go/grpc_server && go build ./... && go test ./...` (after each task) | PASS |
| `cd go/grpc_server && go vet ./...` (Tasks 2 and 3) | PASS (no findings) |
| `bash test/package_mode/test-policy-wiring.sh` (after Task 1 and again after final commit) | PASS |
| `git diff --ignore-all-space --stat` on every touched file before each `git add` | Used to separate the repo-wide CRLF/LF churn already present in the working tree from the real per-commit diff; every commit's final `git diff --cached --stat` matches its `--ignore-all-space` size |
| `go test ./... -v` (full run, all three commits' tests together) | PASS -- 29 tests in `grpc_server`, 4 in `grpc_server/auth` |

## Deferred / Could Not Verify

- **A full C++ compile of `mainwindow_grpc.cpp` against the regenerated `libcore.pb.h`:** Qt 6 and the C++ protobuf toolchain are not available on this machine; `cmake/myproto.cmake` generates the C++ protobuf sources at build time from the same `.proto` this plan edited. Per the environment notes, this is proven in CI by `build-cpp` (ubuntu-22.04, windows-latest) and `package-policy-tests`. `set_channel`/`set_download_dir` follow the same snake_case-to-`set_<field>` convention every other field in this file already uses (`set_action`, `set_check_pre_release`), so this is a naming-convention risk only, not a logic risk.
- **The real `Check` RPC against the live GitHub API on a deb install, and a real download whose checksum matches:** explicitly UAT per the plan's own `<verification>` section, not CI-coverable.
- **The digest-mismatch path against an actual corrupted asset:** per the plan's own note, a deliberately corrupted asset cannot be produced against a published release, so this is proven by `TestVerifyAssetChecksumFailsOnMismatch` (a pure-function unit test) rather than an end-to-end download.

## State of the Owner's Uncommitted Work

Confirmed still present and still uncommitted after all three commits:
- `src/ui/mainwindow.h`: `#include <QElapsedTimer>` and `QElapsedTimer connectionElapsedTimer;` member
- `src/ui/mainwindow_grpc.cpp`: `if (!connectionElapsedTimer.isValid()) connectionElapsedTimer.start();` in `proxor_start` -- temporarily removed and re-applied around each of this plan's two `mainwindow_grpc.cpp` commits (Task 1 and Task 2), verified by `git diff -- src/ui/mainwindow_grpc.cpp` after each restore to show only that one owner line outstanding
- `src/sub/GroupUpdater.cpp`, `src/ui/model/GroupListModel.cpp`, `src/ui/dialog_manage_groups.cpp`, `go.sum`: untouched, still dirty
- The repo-wide CRLF-vs-LF churn noted in the task context remains present and untouched outside the files this plan intentionally edited; each touched file's line-ending convention was matched to its own HEAD convention (some files in this repo are committed LF, some CRLF) before staging, rather than normalized wholesale

## Known Stubs

None introduced by this plan. `UpdateReq.download_dir` is set end-to-end for the one channel that needs it (AppImage) and left empty everywhere else, matching today's behaviour exactly; there is no stub path.

## Self-Check

FOUND: go/grpc_server/update.go (checksumForAsset defined)
FOUND: go/grpc_server/update.go (suffixesForChannel defined)
FOUND: go/grpc_server/update.go (downloadDestination defined)
FOUND: go/grpc_server/gen/libcore.proto (download_dir field)
FOUND: commit 1fd66da7
FOUND: commit e44f6875
FOUND: commit dc5dec1c

## Self-Check: PASSED

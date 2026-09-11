---
phase: 02-cross-platform-package-distribution
plan: 09
status: partial
subsystem: flatpak-bundle
tags: [flatpak, sandbox, ci]
requires: [02-08]
provides: [restricted manifest, offline input preparer, static CI contract]
affects: [02-10]
completed: 2026-09-11
---

# Phase 02 Plan 09: Restricted Flatpak Bundle Summary

The restricted Flatpak manifest, offline build input preparer, private `/app` payload layout, and acceptance contract are committed without granting host networking privileges.

## Task Commits
1. `630faa55` — failing restricted Flatpak acceptance contract.
2. `b5a88a06` — manifest, wrapper, offline input preparation, and payload staging.
3. `8ddae033` — workflow static validation and release fan-in guard.

## Verification
- `bash -n packaging/flatpak/{proxor-wrapper.sh,build-offline.sh,prepare-build-inputs.sh,tests/test-source-closure.sh,tests/test-flatpak-package.sh}`
- `bash packaging/flatpak/tests/test-source-closure.sh`
- `bash packaging/flatpak/tests/test-flatpak-package.sh`
- Workflow YAML parses with Ruby.

## Deferred Issues
- `flatpak-builder`, an approved KDE SDK runtime, and Xvfb are unavailable on this macOS executor. The workflow currently validates source/permission contracts only; a release runner must perform the real normal build, `--disable-download` rebuild, bundle install, and startup before a `.flatpak` asset can be published.
- The release fan-in deliberately fails closed without that real `.flatpak` artifact, preventing a partial GitHub Release upload.

## Self-Check: PASSED

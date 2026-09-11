---
phase: 02-cross-platform-package-distribution
plan: 08
status: complete
subsystem: flatpak-source-closure
tags: [flatpak, appstream, go, provenance]
requires: [02-12]
provides: [Flatpak identity metadata, deterministic Go archive closure]
affects: [02-09]
key-decisions:
  - "Go module archives are checksum-pinned from the module proxy and materialized before the offline builder run."
completed: 2026-09-11
---

# Phase 02 Plan 08: Flatpak Source Closure Summary

Flatpak now has its own AppStream identity and a generated, checksum-pinned Go module archive closure for the recursive source build.

## Task Commits
1. `01155dce` — failing Flatpak source-closure contract.
2. `d24fd604` — Flatpak desktop/AppStream metadata and generated source closure.

## Verification
- `bash -n packaging/flatpak/generate-go-sources.sh packaging/flatpak/tests/test-source-closure.sh`
- `bash packaging/flatpak/tests/test-source-closure.sh`
- Generated closure contains 337 source records, including the recursive source contract and 336 module archives.

## Deviations from Plan

### Auto-fixed Issues
1. **[Rule 3 - Blocking] Generated closure from an isolated clean worktree**
   - The owner-held dirty Go dependency files cannot safely be modified or used as a release lock.
   - A disposable clean worktree with recursive submodules produced the checked-in archive hashes; no owner files were staged or changed.

## Known Constraints
- `generate-go-sources.sh --check` must run against the clean committed Go locks. It intentionally differs from the owner-held dirty worktree and is therefore a CI/clean-checkout validation.

## Self-Check: PASSED

---
phase: 02-cross-platform-package-distribution
plan: 12
subsystem: package-provenance
tags: [source, provenance, CI]
requires: []
provides: [clean recursive source archive, immutable package workflow provenance]
affects: [02-04, 02-05, 02-06, 02-07, 02-08, 02-09]
tech-stack:
  added: [git archive, SHA-256 provenance]
  patterns: [commit-pinned source staging, action SHA pinning]
key-files:
  created: [packaging/source/stage-recursive-source.sh, packaging/source/tests/test-stage-recursive-source.sh, packaging/toolchains/PROVENANCE.md]
  modified: [.github/workflows/build-proxor-cmake.yml]
key-decisions:
  - "Package source staging is derived only from a clean exact commit with recursive submodules."
  - "Package workflow actions are pinned to immutable commits."
requirements-completed: [DIST-03, DIST-04, DIST-05, DIST-06, DIST-07]
completed: 2026-09-11
---

# Phase 02 Plan 12: Source and Toolchain Provenance Summary

**Package builders receive one commit-pinned recursive archive with verified action and container provenance.**

## Accomplishments
- Stages a clean `proxor-X.Y.Z` source tree and archive with sing-box, QHotkey, and SQLiteCpp, excluding Git metadata and dirty files.
- Emits a source manifest with commit, version, submodule commits, and archive SHA-256.
- Pins package workflow actions and validates their provenance record before packaging.

## Task Commits
1. `f49fb28f` — immutable package source contract tests.
2. `341fa7aa` — verified recursive source staging and source artifact upload.
3. `a7f15ada` — remaining package workflow action provenance fixes.

## Verification
- `bash -n packaging/source/stage-recursive-source.sh packaging/source/tests/test-stage-recursive-source.sh`
- `bash packaging/source/tests/test-stage-recursive-source.sh`
- `ruby -e "require 'yaml'; YAML.load_file('.github/workflows/build-proxor-cmake.yml')"`

## Deviations from Plan

### Auto-fixed Issues
1. **[Rule 1 - Bug] Pinned previously mutable package workflow actions**
   - The source test only checked checkout/setup-go pins while package jobs still used mutable action tags.
   - Pinned the remaining package actions and expanded provenance validation.
   - Commit: `a7f15ada`.

## Known Constraints
- Windows UAT is explicitly deferred by user instruction; no Windows runtime claim is made from this macOS executor.

## Self-Check: PASSED
- Confirmed source staging files and commits `f49fb28f`, `341fa7aa`, and `a7f15ada` exist.

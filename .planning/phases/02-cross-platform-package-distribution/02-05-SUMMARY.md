---
phase: 02-cross-platform-package-distribution
plan: 05
status: implemented-ci-pending
---
# Phase 02 Plan 05: Debian Package Summary

Native debhelper metadata, a staged-source `dpkg-buildpackage` wrapper, and a pinned Debian 12 install/start acceptance driver are committed.

## Task Commits
- `7664ea8c` Debian acceptance contract
- `9b00adbc` executable mode correction
- `e3499b8c` Debian metadata and build wrapper
- `4243e1ef` Ubuntu/Debian CI job

## Validation
- `ruby -e "require 'yaml'; YAML.load_file('.github/workflows/build-proxor-cmake.yml')"`
- `bash -n packaging/debian/build-deb.sh packaging/debian/tests/test-deb-package.sh`

## CI-only gates
Docker is unavailable on this executor. CI must build in pinned Ubuntu 22.04 and install/smoke the `.deb` in pinned Debian 12.

## Self-Check: PASSED

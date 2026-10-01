#!/usr/bin/env bash
set -euo pipefail
repo_root="$1"
fail() { echo "linux_desktop.sh: $1" >&2; exit 1; }

lin="$repo_root/cmake/linux/linux.cmake"
grep -qF 'src/sys/linux/LinuxSystemProxy.cpp' "$lin" || fail "linux.cmake must list LinuxSystemProxy.cpp"
grep -qF 'set(PLATFORM_REPLACED_SOURCES 3rdparty/qv2ray/v2/components/proxy/QvProxyConfigurator.cpp)' "$lin" \
  || fail "linux.cmake must replace QvProxyConfigurator.cpp"
grep -qF 'list(REMOVE_ITEM PROJECT_SOURCES ${PLATFORM_REPLACED_SOURCES})' "$repo_root/CMakeLists.txt" \
  || fail "CMakeLists.txt lost the PLATFORM_REPLACED_SOURCES hook"
for os in windows macos; do
  if grep -qE 'PLATFORM_REPLACED_SOURCES|LinuxSystemProxy' "$repo_root/cmake/$os/$os.cmake"; then fail "$os.cmake must not mention the Linux replacement"; fi
done

sp="$repo_root/src/sys/linux/LinuxSystemProxy.cpp"
for needle in PlanLinuxProxyClear PlanLinuxProxySet ReportSystemProxyProblem; do
  grep -qF "$needle" "$sp" || fail "LinuxSystemProxy.cpp lost $needle"
done
if grep -qF '"kwriteconfig5"' "$sp"; then fail "the KDE tool must come from the planner"; fi
grep -qF 'TakeSystemProxyProblem' "$repo_root/src/ui/mainwindow.cpp" || fail "mainwindow.cpp must show the failed-clear warning"

if [ -n "$(git -C "$repo_root" diff --name-only "$(cd "$repo_root" && sh .planning/phases/52-linux-desktop-integration/phase-base.sh)" -- 3rdparty/qv2ray)" ]; then
  fail "3rdparty/qv2ray must stay untouched"
fi
if grep -n 'Q_OS_' "$sp" "$repo_root/src/platform/LinuxDesktop.cpp" "$repo_root/src/platform/LinuxSystemProxyPlan.cpp" >&2; then
  fail "no Q_OS_ in the Linux desktop sources"
fi
echo "linux_desktop: OK"

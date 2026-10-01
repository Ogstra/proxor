#!/usr/bin/env bash
set -euo pipefail
repo_root="$1"
ar="$repo_root/src/sys/AutoRun.cpp"
mn="$repo_root/src/main/main.cpp"

grep -qF 'LinuxAutostartCommand(' "$ar" || { echo "autostart.sh: AutoRun.cpp must build the command through LinuxAutostartCommand" >&2; exit 1; }
grep -qF 'LinuxAutostartDesktopEntry(' "$ar" || { echo "autostart.sh: AutoRun.cpp must write the entry through LinuxAutostartDesktopEntry" >&2; exit 1; }
if grep -qF 'appCmdList << QApplication::applicationFilePath()' "$ar"; then
  echo "autostart.sh: AutoRun.cpp still builds the command inline" >&2
  exit 1
fi
n="$(grep -c '^QString AutoRun_RefreshStaleEntry()' "$ar")"
[ "$n" = 3 ] || { echo "autostart.sh: AutoRun_RefreshStaleEntry defined $n times, expected 3" >&2; exit 1; }
grep -qF 'AutoRun_RefreshStaleEntry()' "$mn" || { echo "autostart.sh: main.cpp does not call AutoRun_RefreshStaleEntry" >&2; exit 1; }
for s in packaging/debian/tests/test-deb-package.sh packaging/rpm/tests/test-rpm-package.sh; do
  grep -qF 'linux-autostart-native.desktop' "$repo_root/$s" || { echo "autostart.sh: $s does not run the autostart fixture" >&2; exit 1; }
done
echo "autostart: OK"

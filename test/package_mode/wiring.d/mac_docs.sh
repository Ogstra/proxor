#!/usr/bin/env bash
set -euo pipefail
repo_root="$1"
option=B   # MAC-AUTORUN decision in 53-RESEARCH.md
route=R2   # MAC-DOCK decision in 53-RESEARCH.md
# MAC-THEME decision: System hidden, Fusion default, follows light/dark
fail() { echo "mac_docs.sh: $*" >&2; exit 1; }
tm="$repo_root/src/ui/ThemeManager.cpp"
doc="$repo_root/docs/Build_macOS.md"
dlg="$repo_root/src/ui/dialog_basic_settings.cpp"

# System entry only outside macOS.
awk '
  /QStringLiteral\("System"\), QStringLiteral\("System"\)\}/ { found=1; if (prev != "#ifndef Q_OS_MACOS") bad=1 }
  NF { prev=$0 }
  END { exit (found && !bad) ? 0 : 1 }' "$tm" || fail "System theme entry must follow #ifndef Q_OS_MACOS in ThemeManager.cpp"

# NormalizeTheme maps System to Fusion inside a Q_OS_MACOS range.
awk '
  /^#ifdef Q_OS_MACOS/ { m=1 }
  /^#(else|endif)/ { m=0 }
  m && /if \(normalized == QStringLiteral\("System"\)\) return QStringLiteral\("Fusion"\);/ { f=1 }
  END { exit f ? 0 : 1 }' "$tm" || fail "NormalizeTheme must map System to Fusion under Q_OS_MACOS"

# Settings note only under Q_OS_MACOS.
awk '
  /^#ifdef Q_OS_MACOS/ { m=1 }
  /^#(else|endif)/ { m=0 }
  m && /The native macOS \(System\) theme is not available yet/ { f=1 }
  END { exit f ? 0 : 1 }' "$dlg" || fail "dialog_basic_settings.cpp needs the Appearance note inside #ifdef Q_OS_MACOS"

for s in '## Desktop integration' 'brew upgrade --cask proxor' 'Screen Recording' \
         'follows the macOS light/dark appearance' 'Install channel: homebrew'; do
  grep -qF -- "$s" "$doc" || fail "docs/Build_macOS.md must contain: $s"
done
for s in 'System theme is the native macOS look' 'Install channel: portable' 'an app icon (`.icns`)'; do
  if grep -qF -- "$s" "$doc"; then fail "docs/Build_macOS.md must not contain: $s"; fi
done

case "$option" in
  B) grep -qF 'LaunchAgents/io.github.Ogstra.Proxor.autostart.plist' "$doc" || fail "docs must name the LaunchAgent" ;;
  A) grep -qF 'Login Items' "$doc" || fail "docs must mention Login Items" ;;
  C) grep -qF 'macOS Start with system does NOT work yet' "$doc" || fail "docs must say Start with system does not work" ;;
esac
case "$route" in
  R3) grep -qF 'does NOT work yet' "$doc" || fail "docs must say the Dock click does not work yet" ;;
  *) grep -qF 'clicking the Dock icon brings the main window back' "$doc" || fail "docs must say the Dock click brings the window back" ;;
esac
for s in 'macos-x86_64.zip' 'macOS 12' 'build_macos_intel.sh' 'PROXOR_MACOS_ARCH=x86_64' \
         'check_macos_availability.sh' 'arch -x86_64' 'MACOS_INTEL_REQUIRED'; do
  grep -qF -- "$s" "$doc" || fail "docs/Build_macOS.md must contain: $s"
done
if grep -qF 'is a universal binary' "$doc"; then fail "docs/Build_macOS.md must not say the Intel build is a universal binary"; fi
for s in 'proxor-app-update.sh' '.proxor-update' 'Download and Restart' 'Download manually'; do
  grep -qF -- "$s" "$doc" || fail "docs/Build_macOS.md must contain: $s"
done
for s in 'There is no in-app download on macOS' 'macOS in-app self-update does NOT work yet' 'a DMG installer and in-app self-update'; do
  if grep -qF -- "$s" "$doc"; then fail "docs/Build_macOS.md must not contain: $s"; fi
done
grep -qF 'update-e2e-macos' "$repo_root/packaging/PUBLISHING.md" || fail "packaging/PUBLISHING.md must list update-e2e-macos"
echo "mac_docs: OK"

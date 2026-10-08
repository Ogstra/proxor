#!/usr/bin/env bash
set -euo pipefail
repo_root="$1"
fail() { echo "mac_autostart_ui.sh: $1" >&2; exit 1; }
option=B # the letter on the MAC-AUTORUN: line of 53-RESEARCH.md ## Decisions

dlg="$repo_root/src/ui/dialog_basic_settings.cpp"
caps="$repo_root/src/platform/PlatformCapabilities.cpp"

# True when every line matching $2 in $1 sits inside an #ifdef Q_OS_MACOS ... #else/#endif range.
only_in_macos() {
  awk -v pat="$2" '
    /^#[ \t]*if/ { depth++; if ($0 ~ /Q_OS_MACOS/ && $0 !~ /!/) mac[depth] = 1; else mac[depth] = 0 }
    /^#[ \t]*else/ { if (mac[depth]) mac[depth] = 0 }
    /^#[ \t]*endif/ { mac[depth] = 0; depth-- }
    index($0, pat) { found = 1; if (!mac[depth]) bad = 1 }
    END { exit (found && !bad) ? 0 : 1 }' "$1"
}

case "$option" in
  C)
    grep -qF 'OpenLoginItemsSettings(' "$dlg" || fail "Settings must offer the Login Items button"
    grep -qF 'does NOT work yet' "$caps" || fail "PlatformCapabilities must carry the Option C reason"
    ;;
  *)
    for tok in 'DecideMacAutostartView(' 'OpenLoginItemsSettings(' 'mac_autostart_loaded'; do
      only_in_macos "$dlg" "$tok" || fail "$tok must appear in dialog_basic_settings.cpp only inside #ifdef Q_OS_MACOS"
    done
    grep -qF '        AutoRun_SetEnabled(ui->start_with_system->isChecked());' "$dlg" || fail "the shared AutoRun_SetEnabled line must stay for the non-macOS path"
    if grep -qF 'Start with system is not available on macOS yet' "$caps"; then fail "PlatformCapabilities must offer Start with system on macOS"; fi
    only_in_macos "$repo_root/src/ui/dialog_basic_settings.h" 'mac_autostart_loaded' || fail "the header member must be inside #ifdef Q_OS_MACOS"
    ;;
esac
echo "mac_autostart_ui: OK"

#!/usr/bin/env bash
set -euo pipefail
repo_root="$1"
fail() { echo "update_fallback.sh: $*" >&2; exit 1; }

# True when the needle sits between #ifdef Q_OS_WIN and the next #else/#endif.
in_win() {
  awk -v needle="$1" '/^#ifdef Q_OS_WIN/{m=1; next} /^#(else|endif)/{m=0} m && index($0, needle){f=1} END{exit !f}' "$2"
}
# True when the needle sits between the #else and #endif of a block opened by #ifdef Q_OS_WIN.
in_win_else() {
  awk -v needle="$1" '/^#ifdef Q_OS_WIN/{w=1; e=0; next} /^#else/{if(w){e=1}; next} /^#endif/{w=0; e=0; next} e && index($0, needle){f=1} END{exit !f}' "$2"
}

grpc="$repo_root/src/ui/mainwindow_grpc.cpp"
mw="$repo_root/src/ui/mainwindow.cpp"
mwh="$repo_root/src/ui/mainwindow.h"

in_win 'ShowUpdateFailedDialog(this, UpdateFailureStage::Check' "$grpc" || fail "Check-stage dialog must be inside #ifdef Q_OS_WIN"
n=$(grep -c 'UpdateFailureStage::Download' "$grpc" || true)
[ "$n" -ge 2 ] || fail "expected at least 2 Download-stage dialog calls, found $n"
while IFS= read -r line; do
  in_win "$line" "$grpc" || fail "Download-stage call outside #ifdef Q_OS_WIN: $line"
done < <(grep 'UpdateFailureStage::Download' "$grpc")
in_win 'update_release_url = releasePageUrl.toString();' "$grpc" || fail "update_release_url capture must be inside #ifdef Q_OS_WIN"
in_win_else 'MessageBoxWarning(QObject::tr("Update"), err.c_str());' "$grpc" || fail "original check-error box must stay in the #else branch"
in_win_else 'MessageBoxWarning(QObject::tr("Update"), response2.error().c_str());' "$grpc" || fail "original download-error box must stay in the #else branch"
grep -qxF '                    if (!ok2) return;' "$grpc" || fail "the plain if (!ok2) return; must remain"
s=$(grep -c 'if (silent) return;' "$grpc" || true)
[ "$s" -ge 2 ] || fail "both silent guards must be kept"

in_win 'ShowUpdateFailedDialog(this, UpdateFailureStage::Install' "$mw" || fail "Install-stage dialog must be inside #ifdef Q_OS_WIN"
in_win '#include "ui/dialog_update_available.h"' "$mw" || fail "the dialog include must be inside #ifdef Q_OS_WIN"
in_win_else 'MessageBoxWarning(software_name, tr("%1 The app will stay open.").arg(launch.reason));' "$mw" || fail "original install box must stay in the #else branch"
in_win 'QString update_release_url;' "$mwh" || fail "update_release_url must be inside #ifdef Q_OS_WIN"

expected="$repo_root/src/ui/dialog_update_available.cpp
$repo_root/src/ui/mainwindow.cpp
$repo_root/src/ui/mainwindow_grpc.cpp"
found=$(grep -rl 'ShowUpdateFailedDialog(' "$repo_root/src" | grep -v 'dialog_update_available\.h$' | LC_ALL=C sort || true)
[ "$found" = "$expected" ] || fail "unexpected ShowUpdateFailedDialog( users: $found"
echo "update_fallback: OK"

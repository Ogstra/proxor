#!/usr/bin/env bash
set -euo pipefail
repo_root="$1"
fail() { echo "update_dialog.sh: $*" >&2; exit 1; }

h="$repo_root/src/ui/dialog_update_available.h"
c="$repo_root/src/ui/dialog_update_available.cpp"

for f in "$h" "$c"; do
  if grep -qF -- 'Q_OS_' "$f"; then fail "$f must stay free of Q_OS_"; fi
done

for pat in 'class DialogUpdateFailed' 'void ShowUpdateFailedDialog(' \
           'enum class UpdateFailureStage { Check, Download, Install };' '"https://github.com/Ogstra/proxor/releases"'; do
  grep -qF -- "$pat" "$h" || fail "header must contain: $pat"
done
for pat in buttonDownloadManually lineEditManualUrl labelUpdateFailedKeepsWorking labelUpdateFailedError WA_DeleteOnClose; do
  grep -qF -- "$pat" "$c" || fail "dialog_update_available.cpp must contain: $pat"
done

if grep -rqF -- 'dialog_update_failed' "$repo_root/CMakeLists.txt" "$repo_root/cmake"; then
  fail "the dialog must not get its own CMake entry"
fi
echo "update_dialog: OK"

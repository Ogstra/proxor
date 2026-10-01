#!/usr/bin/env bash
set -euo pipefail
repo_root="$1"
mw="$repo_root/src/ui/mainwindow.cpp"
dlg="$repo_root/src/ui/dialog_ssid_settings.cpp"

fail() { echo "wifi_ui.sh: $1" >&2; exit 1; }
has() { grep -q -- "$2" "$1" || fail "$(basename "$1") must contain $2"; }

for p in 'ProxorWifi::MonitoringNeeded(' 'ProxorWifi::HostsSkipDiffers(' 'ProxorWifi::DecidePermissionPrompt(' 'RequestWifiPermission('; do
  has "$mw" "$p"
done
! grep -q 'wifi_monitor->setActive(true)' "$mw" || fail "monitor must not be activated unconditionally"
awk '/^void MainWindow::dialog_message_impl/,/^}/' "$mw" | grep -q 'refreshWifiMonitoring()' \
  || fail "dialog_message_impl must call refreshWifiMonitoring()"

body="$(awk '/^void MainWindow::onWifiSsidChanged/,/^}/' "$mw")"
echo "$body" | grep -q 'const bool viaTrigger = started_via_ssid_trigger' || fail "onWifiSsidChanged must capture viaTrigger"
echo "$body" | grep -q 'proxor_start([^)]*viaTrigger' || fail "hosts restart must pass viaTrigger"
! echo "$body" | grep -q 'proxor_start(ProxorGui::dataStore->started_id)' || fail "no single-argument restart"
a="$(echo "$body" | grep -n 'applyOnDemandForSsid(' | head -1 | cut -d: -f1)"
b="$(echo "$body" | grep -n 'HostsSkipDiffers(' | head -1 | cut -d: -f1)"
[ -n "$a" ] && [ -n "$b" ] && [ "$a" -lt "$b" ] || fail "applyOnDemandForSsid must run before HostsSkipDiffers"

for p in 'WifiMonitor::appInstance()' 'refreshNow()' 'RequestWifiPermission(' 'OpenWifiPermissionSettings(' 'btn_add_current_ssid'; do
  has "$dlg" "$p"
done
! grep -q 'Q_OS_' "$dlg" || fail "dialog_ssid_settings.cpp must not use Q_OS_"
echo "wifi_ui: OK"

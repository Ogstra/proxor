#!/usr/bin/env bash
set -euo pipefail
repo_root="$1"
ui="$repo_root/src/ui/mainwindow.ui"
mw="$repo_root/src/ui/mainwindow.cpp"
for n in menu_scan_qr_image menu_scan_qr_clipboard; do
  grep -qF "name=\"$n\"" "$ui" || { echo "qr.sh: mainwindow.ui missing action $n" >&2; exit 1; }
  grep -qF "<addaction name=\"$n\"/>" "$ui" || { echo "qr.sh: mainwindow.ui menu missing addaction $n" >&2; exit 1; }
done
for pat in 'Capability::ScreenQrCapture' 'DecodeQrFromImage(' 'QrSource::ClipboardImage' 'QrSource::ImageFile' 'QGuiApplication::screens()'; do
  grep -qF "$pat" "$mw" || { echo "qr.sh: mainwindow.cpp missing $pat" >&2; exit 1; }
done
n=$(awk '/#ifdef NKR_NO_ZXING/,/#endif/' "$mw" | grep -c 'setVisible(false)' || true)
[ "$n" -ge 3 ] || { echo "qr.sh: NKR_NO_ZXING block must hide all three QR actions" >&2; exit 1; }
if grep -qE 'MessageBoxInfo\([^)]*tr\("QR Code not found"\)' "$mw"; then
  echo "qr.sh: the literal QR Code not found message is back" >&2; exit 1
fi

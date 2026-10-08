#!/usr/bin/env bash
set -euo pipefail
[ "$#" = 1 ] || { echo "Usage: $0 <proxor*.rpm>" >&2; exit 2; }
rpm="$1"; [ -f "$rpm" ] || exit 1
# A release tag repeated in the file name, as in 1.6.5-1.fc44.fc44, means the spec and
# the platform both appended the dist tag.
basename "$rpm" | grep -Eq '^proxor-[0-9]+\.[0-9]+\.[0-9]+-[0-9]+\.fc[0-9]+\.x86_64\.rpm$' || {
  echo "unexpected release tag in $(basename "$rpm")" >&2; exit 1; }
image="fedora@sha256:43b29f65a41eb9c35e1cd5323e3bdf3b655c2357a9f4f1ff2f9c2798e5045d80"
dir="$(CDPATH= cd -- "$(dirname "$rpm")" && pwd)"; name="$(basename "$rpm")"
config="$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)/proxor.rpmlint.toml"
[ -f "$config" ] || exit 1
fixture="$(CDPATH= cd -- "$(dirname "$0")/../../.." && pwd)/test/package_mode/fixtures/linux-autostart-native.desktop"
[ -f "$fixture" ] || { echo "autostart fixture not found: $fixture" >&2; exit 1; }
docker run --rm -v "$dir:/packages:ro" -v "$fixture:/fixtures/autostart.desktop:ro" -v "$config:/rpmlint/proxor.rpmlint.toml:ro" "$image" bash -ceu '
  dnf -y install rpm-build rpmlint desktop-file-utils xorg-x11-server-Xvfb xauth
  rpm=/packages/$1; rpm -qpl "$rpm"; rpm -qpR "$rpm"; rpm -qp --scripts "$rpm"; rpmlint --config /rpmlint/proxor.rpmlint.toml "$rpm"
  for p in /usr/bin/proxor /usr/lib/proxor/proxor /usr/lib/proxor/proxor_core /usr/share/proxor/geoip.dat /usr/share/proxor/geosite.dat /usr/share/proxor/geoip.db /usr/share/proxor/geosite.db /usr/share/proxor/package-channel /usr/share/applications/proxor.desktop /usr/share/icons/hicolor/256x256/apps/proxor.png; do rpm -qpl "$rpm" | grep -qx "$p"; done
  ! rpm -qpl "$rpm" | grep -Eqi "AppDir|linuxdeploy|updater|plugins/|qt[0-9]"; ! rpm -qp --scripts "$rpm" | grep -Eqi "setcap|cap_net_admin"
  dnf -y install "$rpm"
  grep -qx rpm /usr/share/proxor/package-channel
  desktop-file-validate /usr/share/applications/proxor.desktop
  grep -q QT_PLUGIN_PATH /usr/bin/proxor
  # The icons are SVG, so the reader plugins have to come with the dependencies. Their
  # directory differs between distributions, hence the search and the listing on failure.
  for plugin in libqsvgicon.so libqsvg.so; do
    find /usr/lib64/qt6/plugins /usr/lib/qt6/plugins -name "$plugin" 2>/dev/null | grep -q . || {
      echo "missing Qt plugin $plugin"; find /usr/lib64/qt6/plugins /usr/lib/qt6/plugins -name '*.so' 2>/dev/null | sort; exit 1; }
  done
  set +e; xvfb-run -a timeout 10s /usr/bin/proxor -many; rc=$?; set -e; test "$rc" = 0 -o "$rc" = 124
  # The launch legitimately ends in a 124 timeout; the startup log line is written
  # during init regardless, so the channel assertion must not depend on the exit code.
  # The app keeps its configuration beside the executable while that prefix is writable and
  # falls back to the application data directory of the user when it is not. This container
  # runs as root, so the prefix is writable here while a real install takes the other path.
  # Accept either. No apostrophes in here: this whole script body is single-quoted.
  log="$(ls -t /usr/lib/proxor/config/logs/proxor-*.log "$HOME"/.config/proxor/config/logs/proxor-*.log 2>/dev/null | head -n1)"
  if [ -z "$log" ]; then
    echo "no startup log beside the executable or under $HOME/.config/proxor"
    find / -maxdepth 6 -name 'proxor-*.log' 2>/dev/null | head
    exit 1
  fi
  grep -q "Install channel: rpm" "$log" || {
    echo "no channel line in $log"; cat "$log"; exit 1; }
  # G-02: the autostart entry the app writes for native packages must start Proxor.
  # The entry comes from the committed fixture, so a change there forces a change here.
  desktop-file-validate /fixtures/autostart.desktop
  exec_line="$(sed -n "s/^Exec=//p" /fixtures/autostart.desktop)"
  test "$exec_line" = "\"/usr/bin/proxor\" \"-tray\" \"-appdata\""
  rm -rf "$HOME/.config/proxor"
  set +e
  xvfb-run -a timeout 10s /usr/bin/proxor -tray -appdata -many > /tmp/autostart-run.log 2>&1
  rc=$?
  set -e
  test "$rc" -eq 0 -o "$rc" -eq 124 || { cat /tmp/autostart-run.log; exit 1; }
  ! grep -Eq "could not find the Qt platform plugin|could not load the Qt platform plugin" /tmp/autostart-run.log
  alog="$(ls -t "$HOME"/.config/proxor/config/logs/proxor-*.log 2>/dev/null | head -n1)"
  if [ -z "$alog" ]; then
    echo "no startup log from the autostart command"
    find / -maxdepth 7 -name "proxor-*.log" 2>/dev/null | head
    cat /tmp/autostart-run.log
    exit 1
  fi
  grep -q "Install channel: rpm" "$alog" || { echo "no channel line in $alog"; cat "$alog"; exit 1; }
  # For the record only (not asserted): the old entry ran the GUI binary directly.
  set +e; xvfb-run -a timeout 5s /usr/lib/proxor/proxor -many > /tmp/direct-run.log 2>&1; echo "direct launch rc=$?"; set -e
  grep -i "platform plugin" /tmp/direct-run.log || true
' bash "$name"

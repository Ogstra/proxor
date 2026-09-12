#!/usr/bin/env sh
set -eu

# The embedded qt.conf is for portable layouts, so it points Qt at a prefix that does
# not exist in the sandbox and no platform plugin is found. The runtime ships the
# plugins, exactly like the native packages do.
for plugins in /usr/lib/*/qt6/plugins /usr/lib64/qt6/plugins; do
    if [ -d "$plugins" ]; then
        export QT_PLUGIN_PATH="$plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"
        export QT_QPA_PLATFORM_PLUGIN_PATH="$plugins/platforms"
        break
    fi
done

exec /app/lib/proxor/proxor "$@"

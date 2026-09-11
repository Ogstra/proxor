#!/bin/sh

# The embedded qt.conf is for portable layouts; native packages use the system Qt plugins.
for platform_plugins in /usr/lib/*/qt6/plugins/platforms /usr/lib64/qt6/plugins/platforms; do
    if [ -d "$platform_plugins" ]; then
        export QT_QPA_PLATFORM_PLUGIN_PATH="$platform_plugins"
        break
    fi
done

exec /usr/lib/proxor/proxor "$@"

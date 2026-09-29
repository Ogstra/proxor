#!/bin/sh

# The embedded qt.conf points Qt at "plugins" beside the executable, which is what the
# portable builds ship. A native package has no such directory, so without this every
# plugin category is lost: not only the platform plugin, but the widget style, the SVG
# icon engine and image format, and the TLS backend.
for plugins in /usr/lib/*/qt6/plugins /usr/lib64/qt6/plugins; do
    if [ -d "$plugins/platforms" ]; then
        export QT_PLUGIN_PATH="$plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"
        export QT_QPA_PLATFORM_PLUGIN_PATH="$plugins/platforms"
        break
    fi
done

exec /usr/lib/proxor/proxor "$@"

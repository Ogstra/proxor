#!/usr/bin/env sh
set -eu

# The embedded qt.conf is for portable layouts: it looks for "plugins" beside the
# executable. The build links that name at the runtime's plugin directory; exporting
# it as well keeps the search path explicit for the platform plugin.
if [ -d /app/lib/proxor/plugins/platforms ]; then
    export QT_PLUGIN_PATH="/app/lib/proxor/plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"
    export QT_QPA_PLATFORM_PLUGIN_PATH=/app/lib/proxor/plugins/platforms
fi

exec /app/lib/proxor/proxor "$@"

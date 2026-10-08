#pragma once

#include <QString>

void AutoRun_SetEnabled(bool enable);

bool AutoRun_IsEnabled();

// Linux: rewrites a stale autostart entry written by this install, returns a log line.
// Elsewhere returns an empty string.
QString AutoRun_RefreshStaleEntry();

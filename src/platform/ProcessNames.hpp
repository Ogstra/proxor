#pragma once

// Executable names of running processes as sing-box sees them (file name of the executable),
// read from a /proc-like tree. Qt Core only and compiled on every OS so it is tested everywhere
// against a fake /proc; the app only calls it on Linux.

#include <QString>
#include <QStringList>

namespace ProxorPlatform {

// exeTarget: readlink of /proc/<pid>/exe ("" when unreadable); argv0: first NUL-separated field of
// /proc/<pid>/cmdline; comm: /proc/<pid>/comm trimmed.
// Returns the executable file name, or "" when it cannot be known without guessing.
QString ResolveLinuxProcessName(const QString &exeTarget, const QString &argv0, const QString &comm);

struct ProcessNameList {
    QStringList names; // unique, sorted case-insensitively
    int skipped = 0;   // processes whose name could only be a truncated guess
};

ProcessNameList ListLinuxProcessNames(const QString &procRoot = QStringLiteral("/proc"));

} // namespace ProxorPlatform

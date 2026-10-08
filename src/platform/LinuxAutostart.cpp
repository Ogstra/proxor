#include "LinuxAutostart.hpp"

#include <QDir>

namespace ProxorPlatform {

namespace {
bool IsNativeChannel(PackageMode mode) {
    return mode == PackageMode::Deb || mode == PackageMode::Rpm || mode == PackageMode::Arch ||
           mode == PackageMode::NativeUnknownManager;
}

QString Quote(QString value) {
    value.replace('\\', QStringLiteral("\\\\"));
    value.replace('"', QStringLiteral("\\\""));
    return QStringLiteral("\"") + value + QStringLiteral("\"");
}

QString ExecLine(const QStringList &command) {
    QStringList quoted;
    for (const auto &argument : command) quoted << Quote(argument);
    return quoted.join(' ');
}
} // namespace

QStringList LinuxAutostartCommand(const AutostartInputs &in) {
    QStringList cmd;
    if (in.fromLauncher) {
        cmd << in.launcherPath << QStringLiteral("--");
    } else if (IsNativeChannel(in.packageMode) && !in.nativeWrapperPath.isEmpty()) {
        cmd << in.nativeWrapperPath;
    } else if (!in.appImagePath.isEmpty()) {
        cmd << in.appImagePath;
    } else {
        cmd << in.applicationFilePath;
    }
    cmd << QStringLiteral("-tray");
    if (in.useAppdata) {
        cmd << QStringLiteral("-appdata");
        if (!in.appdataDir.isEmpty()) cmd << in.appdataDir;
    }
    return cmd;
}

QString ReplaceDesktopEntryExec(const QString &entryText, const QStringList &command) {
    QStringList lines = entryText.split('\n');
    for (auto &line : lines) {
        if (line.startsWith(QLatin1String("Exec="))) {
            line = QStringLiteral("Exec=") + ExecLine(command);
            break;
        }
    }
    return lines.join('\n');
}

bool IsAutostartEntryDisabled(const QString &entryText) {
    for (const auto &raw : entryText.split('\n')) {
        const QString line = raw.trimmed();
        if (line == QLatin1String("X-GNOME-Autostart-enabled=false") || line == QLatin1String("Hidden=true")) {
            return true;
        }
    }
    return false;
}

QString NativeWrapperPathFor(const QString &packageRoot) {
    return QDir::cleanPath(packageRoot + QStringLiteral("/../../bin/proxor"));
}

QString LinuxAutostartDesktopEntry(const QString &appName, const QStringList &command) {
    const QString nl = QStringLiteral("\n");
    return QStringLiteral("[Desktop Entry]") + nl + QStringLiteral("Name=") + appName + nl +
           QStringLiteral("Exec=") + ExecLine(command) + nl + QStringLiteral("Terminal=false") + nl +
           QStringLiteral("Categories=Network") + nl + QStringLiteral("Type=Application") + nl +
           QStringLiteral("StartupNotify=false") + nl + QStringLiteral("X-GNOME-Autostart-enabled=true") + nl;
}

QStringList ParseDesktopEntryExec(const QString &entryText) {
    QString value;
    bool found = false;
    for (const auto &line : entryText.split('\n')) {
        if (line.startsWith(QLatin1String("Exec="))) {
            value = line.mid(5);
            found = true;
            break;
        }
    }
    if (!found) return {};

    QStringList out;
    QString cur;
    bool have = false;
    bool inQuote = false;
    for (int i = 0; i < value.size(); ++i) {
        const QChar c = value[i];
        if (inQuote) {
            if (c == '\\' && i + 1 < value.size() &&
                QString("\\\"`$").contains(value[i + 1])) {
                cur += value[++i];
            } else if (c == '"') {
                inQuote = false;
            } else {
                cur += c;
            }
        } else if (c == '"') {
            inQuote = true;
            have = true;
        } else if (c == ' ') {
            if (have) out << cur;
            cur.clear();
            have = false;
        } else {
            cur += c;
            have = true;
        }
    }
    if (have) out << cur;
    return out;
}

bool ShouldRefreshAutostart(const QStringList &existing, const QStringList &expected, const QString &ownBinaryPath) {
    if (existing.isEmpty() || existing == expected) return false;
    return existing.first() == ownBinaryPath;
}

QStringList FlatpakAutostartCommandline(bool useAppdata, const QString &appdataDir) {
    QStringList cmd{QStringLiteral("proxor"), QStringLiteral("-tray")};
    if (useAppdata) {
        cmd << QStringLiteral("-appdata");
        if (!appdataDir.isEmpty()) cmd << appdataDir;
    }
    return cmd;
}

bool ShouldRequestFlatpakAutostart(bool enable, bool markerEnabled) {
    return enable != markerEnabled;
}

} // namespace ProxorPlatform

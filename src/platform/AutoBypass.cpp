#include "platform/AutoBypass.hpp"

#include <QRegularExpression>

namespace ProxorPlatform {

QStringList KnownVpnClientProcessNames(HostOs os) {
    switch (os) {
        case HostOs::Windows:
            return {"wireguard.exe", "openvpn.exe", "tailscaled.exe"};
        case HostOs::Linux:
            return {"wireguard-go", "openvpn", "tailscaled"};
        case HostOs::MacOS:
            return {"wireguard-go", "openvpn", "tailscaled", "WireGuard"};
        case HostOs::Other:
            break;
    }
    return {};
}

namespace {
    void AppendUnique(QStringList &list, const QString &value, Qt::CaseSensitivity cs) {
        for (const auto &v: list) {
            if (v.compare(value, cs) == 0) return;
        }
        list << value;
    }
} // namespace

AutoBypassProcesses BuildAutoBypassProcesses(const QStringList &externalPrograms, HostOs os) {
    const bool win = os == HostOs::Windows;
    const auto cs = win ? Qt::CaseInsensitive : Qt::CaseSensitive;
    static const QRegularExpression winAbs(R"(^([A-Za-z]:\\|\\\\))");
    AutoBypassProcesses out;
    QStringList names;
    for (const auto &raw: externalPrograms) {
        QString p = raw.trimmed();
        if (p.isEmpty()) continue;
        if (win) p.replace('/', '\\');
        const int sep = win ? p.lastIndexOf('\\') : p.lastIndexOf('/');
        const QString name = sep >= 0 ? p.mid(sep + 1) : p;
        if (sep >= 0) {
            const bool absolute = win ? winAbs.match(p).hasMatch() : p.startsWith('/');
            if (absolute) AppendUnique(out.processPaths, p, cs);
        }
        if (!name.isEmpty()) AppendUnique(names, name, cs);
    }
    for (const auto &v: KnownVpnClientProcessNames(os)) AppendUnique(names, v, cs);
    out.processNames = names;
    return out;
}

} // namespace ProxorPlatform

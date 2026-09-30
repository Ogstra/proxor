#pragma once

// Which processes Tun must send direct: external cores (by path and file name) and known VPN clients.
// Pure string logic, parameterised by OS, so every OS table is testable on every runner. Qt Core only.

#include "platform/PlatformCapabilities.hpp"

#include <QStringList>

namespace ProxorPlatform {

struct AutoBypassProcesses {
    QStringList processPaths;
    QStringList processNames;
};

QStringList KnownVpnClientProcessNames(HostOs os);

// externalPrograms: the external cores' programs, already made absolute by the caller when they contain a separator.
AutoBypassProcesses BuildAutoBypassProcesses(const QStringList &externalPrograms, HostOs os);

} // namespace ProxorPlatform

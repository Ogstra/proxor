#pragma once

// Decides which global hotkeys to register and what to tell the user about the rest.
// Qt Core only, so it is unit-tested on every CI runner.

#include "platform/PlatformCapabilities.hpp"

#include <QList>
#include <QString>
#include <QStringList>

namespace ProxorPlatform {

struct HotkeyBinding {
    QString action;   // user-visible name, e.g. "Show main window"
    QString sequence; // QKeySequence text
};

struct HotkeyRegistrationPlan {
    QList<HotkeyBinding> toRegister; // non-empty, non-duplicated, only when the capability is usable
    QStringList problems;            // user-facing lines, empty when nothing to report
    bool hasDuplicates = false;
};

HotkeyRegistrationPlan PlanHotkeyRegistration(const QList<HotkeyBinding> &bindings, const CapabilityStatus &globalHotkeys);
QString HotkeyDuplicateText(const QString &sequence, const QStringList &actions);
QString HotkeyRejectedText(const HotkeyBinding &binding);

} // namespace ProxorPlatform

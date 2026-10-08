#include "platform/HotkeyReport.hpp"

#include <QCoreApplication>

namespace ProxorPlatform {

namespace {
QString Norm(const QString &s) { return s.trimmed().toLower(); }
} // namespace

QString HotkeyDuplicateText(const QString &sequence, const QStringList &actions) {
    return QCoreApplication::translate("HotkeyReport",
                                       "%1 is assigned to %2. None of them was registered; give each action its own key.")
        .arg(sequence, actions.join(QCoreApplication::translate("HotkeyReport", " and ")));
}

QString HotkeyRejectedText(const HotkeyBinding &binding) {
    return QCoreApplication::translate("HotkeyReport",
                                       "%1 (%2) could not be registered: another application or the system already uses this key.")
        .arg(binding.action, binding.sequence);
}

HotkeyRegistrationPlan PlanHotkeyRegistration(const QList<HotkeyBinding> &bindings, const CapabilityStatus &globalHotkeys) {
    HotkeyRegistrationPlan plan;
    bool anyConfigured = false;
    for (const auto &b : bindings) {
        if (!Norm(b.sequence).isEmpty()) anyConfigured = true;
    }
    if (!anyConfigured) return plan;

    if (globalHotkeys.support == Support::Unsupported) {
        plan.problems << QCoreApplication::translate("HotkeyReport", "Global hotkeys are off: %1").arg(globalHotkeys.reason);
        return plan;
    }
    if (globalHotkeys.support == Support::Degraded && !globalHotkeys.reason.isEmpty()) {
        plan.problems << globalHotkeys.reason;
    }

    QStringList seenOrder; // normalized, first-seen order
    for (const auto &b : bindings) {
        const auto n = Norm(b.sequence);
        if (!n.isEmpty() && !seenOrder.contains(n)) seenOrder << n;
    }
    for (const auto &n : seenOrder) {
        QStringList actions;
        QString shown;
        for (const auto &b : bindings) {
            if (Norm(b.sequence) != n) continue;
            actions << b.action;
            if (shown.isEmpty()) shown = b.sequence.trimmed();
        }
        if (actions.size() > 1) {
            plan.hasDuplicates = true;
            plan.problems << HotkeyDuplicateText(shown, actions);
        }
    }
    for (const auto &b : bindings) {
        const auto n = Norm(b.sequence);
        if (n.isEmpty()) continue;
        int count = 0;
        for (const auto &o : bindings) count += Norm(o.sequence) == n;
        if (count == 1) plan.toRegister << b;
    }
    return plan;
}

} // namespace ProxorPlatform

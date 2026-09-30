#include "platform/CapabilityUi.hpp"

#include <QLabel>
#include <QPalette>
#include <QWidget>

namespace ProxorPlatform {

QLabel *MakeCapabilityNote(QWidget *parent) {
    auto *note = new QLabel(parent);
    note->setObjectName(QStringLiteral("capability_note"));
    note->setWordWrap(true);
    note->setTextInteractionFlags(Qt::TextSelectableByMouse);
    // Palette-derived subdued color instead of a stylesheet, so every theme stays readable.
    note->setForegroundRole(QPalette::PlaceholderText);
    note->setVisible(false);
    return note;
}

void ApplyCapability(QWidget *control, const CapabilityStatus &status, QLabel *note) {
    const bool supported = status.support == Support::Supported;
    if (control != nullptr && !supported) {
        control->setToolTip(status.reason);
        if (status.support == Support::Unsupported) control->setEnabled(false);
    }
    if (note != nullptr) {
        note->setText(supported ? QString() : status.reason);
        note->setVisible(!supported);
    }
}

void ApplyCapability(const QList<QWidget *> &controls, const CapabilityStatus &status, QLabel *note) {
    for (auto *control : controls) ApplyCapability(control, status, nullptr);
    if (note != nullptr) ApplyCapability(static_cast<QWidget *>(nullptr), status, note);
}

} // namespace ProxorPlatform

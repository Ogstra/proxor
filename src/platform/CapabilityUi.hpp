#pragma once

// Disable-and-explain helper: pages call ApplyCapability() with a capability status and the control
// is disabled (Unsupported) and/or annotated with the reason, so no gap is ever silent.

#include "platform/PlatformCapabilities.hpp"

#include <QList>

class QLabel;
class QWidget;

namespace ProxorPlatform {

// Hidden, word-wrapped label (objectName "capability_note") that ApplyCapability fills with the reason.
QLabel *MakeCapabilityNote(QWidget *parent);

// Unsupported: disables the control and sets its tooltip to the reason (shown on hover only).
// Degraded / NeedsPermission: keeps the control as is and sets its tooltip to the reason.
// Supported: changes nothing on the control (never re-enables it). The note label stays hidden and empty
// in every case: the explanation is never visible text on the page.
void ApplyCapability(QWidget *control, const CapabilityStatus &status, QLabel *note = nullptr);
void ApplyCapability(const QList<QWidget *> &controls, const CapabilityStatus &status, QLabel *note = nullptr);

} // namespace ProxorPlatform

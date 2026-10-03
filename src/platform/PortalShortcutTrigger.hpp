#pragma once
// Qt Core only: convert Proxor's key sequences to the XDG shortcuts format
// (https://specifications.freedesktop.org/shortcuts-spec/latest/).
#include <QString>

namespace ProxorPlatform {

// QKeySequence::PortableText of ONE chord ("Ctrl+Alt+P") -> XDG trigger ("CTRL+ALT+p"); "" when not expressible.
QString PortalTriggerFromKeySequence(const QString &portableText);

} // namespace ProxorPlatform

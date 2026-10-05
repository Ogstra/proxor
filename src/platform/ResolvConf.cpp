#include "ResolvConf.hpp"

namespace ProxorPlatform {

    ResolvConfKind ClassifyResolvConf(const QString &) { return ResolvConfKind::Other; }

    QString DirectDnsResolvedNotice(ResolvConfKind) { return {}; }

    bool DirectDnsNoticeIsWarning(ResolvConfKind) { return false; }

} // namespace ProxorPlatform

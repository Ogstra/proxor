#include "platform/MacLoginItemPolicy.hpp"

#include <QDir>
#include <QXmlStreamReader>

namespace ProxorPlatform {

MacLoginItemStatus MapSMAppServiceStatus(long raw) {
    switch (raw) {
    case 0: return MacLoginItemStatus::NotRegistered;
    case 1: return MacLoginItemStatus::Enabled;
    case 2: return MacLoginItemStatus::RequiresApproval;
    case 3: return MacLoginItemStatus::NotFound;
    default: return MacLoginItemStatus::Unknown;
    }
}

QString MacAutostartLabel() { return QStringLiteral("io.github.Ogstra.Proxor.autostart"); }

MacLaunchAgentSpec DefaultMacLaunchAgentSpec(const QString &appBundlePath, const QString &appdataDir) {
    MacLaunchAgentSpec spec;
    spec.label = MacAutostartLabel();
    spec.bundleId = QStringLiteral("io.github.Ogstra.Proxor");
    spec.appBundlePath = appBundlePath;
    spec.appdataDir = appdataDir;
    return spec;
}

QStringList MacLaunchAgentArguments(const MacLaunchAgentSpec &spec) {
    QStringList args{QStringLiteral("/usr/bin/open"), QStringLiteral("-a"), spec.appBundlePath,
                     QStringLiteral("--args"), QStringLiteral("-tray")};
    if (!spec.appdataDir.isEmpty()) args << QStringLiteral("-appdata") << spec.appdataDir;
    return args;
}

QByteArray MacLaunchAgentPlist(const MacLaunchAgentSpec &spec) {
    auto str = [](const QString &v, int tabs) {
        return QString(tabs, QLatin1Char('\t')) + QStringLiteral("<string>") + v.toHtmlEscaped() +
               QStringLiteral("</string>\n");
    };
    QString x;
    x += QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    x += QStringLiteral("<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
                        "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n");
    x += QStringLiteral("<plist version=\"1.0\">\n<dict>\n");
    x += QStringLiteral("\t<key>AssociatedBundleIdentifiers</key>\n\t<array>\n");
    x += str(spec.bundleId, 2);
    x += QStringLiteral("\t</array>\n");
    x += QStringLiteral("\t<key>Label</key>\n");
    x += str(spec.label, 1);
    x += QStringLiteral("\t<key>LimitLoadToSessionType</key>\n");
    x += str(QStringLiteral("Aqua"), 1);
    x += QStringLiteral("\t<key>ProcessType</key>\n");
    x += str(QStringLiteral("Interactive"), 1);
    x += QStringLiteral("\t<key>ProgramArguments</key>\n\t<array>\n");
    for (const QString &a : MacLaunchAgentArguments(spec)) x += str(a, 2);
    x += QStringLiteral("\t</array>\n");
    x += QStringLiteral("\t<key>RunAtLoad</key>\n\t<true/>\n");
    x += QStringLiteral("</dict>\n</plist>\n");
    return x.toUtf8();
}

QStringList ParseMacLaunchAgentArguments(const QByteArray &plistXml) {
    QXmlStreamReader xml(plistXml);
    QStringList out;
    bool sawKey = false;
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement()) continue;
        if (xml.name() == QLatin1String("key")) {
            sawKey = xml.readElementText() == QLatin1String("ProgramArguments");
            continue;
        }
        if (sawKey && xml.name() == QLatin1String("array")) {
            while (!xml.atEnd()) {
                xml.readNext();
                if (xml.isEndElement() && xml.name() == QLatin1String("array")) break;
                if (xml.isStartElement() && xml.name() == QLatin1String("string")) out << xml.readElementText();
            }
            return xml.hasError() ? QStringList() : out;
        }
        sawKey = false;
    }
    return {};
}

QString MacLaunchAgentTarget(const QStringList &arguments) {
    const int i = arguments.indexOf(QStringLiteral("-a"));
    if (i < 0 || i + 1 >= arguments.size()) return {};
    return arguments.at(i + 1);
}

QString MacLaunchAgentFile(const QString &launchAgentsDir, const QString &label) {
    return launchAgentsDir + QLatin1Char('/') + label + QStringLiteral(".plist");
}

bool ShouldRefreshMacLaunchAgent(const QStringList &existing, const QStringList &expected, bool existingTargetExists) {
    if (existing.isEmpty() || existing == expected) return false;
    return !existingTargetExists;
}

QString MacLoginItemsLocation() { return QStringLiteral("System Settings > General > Login Items & Extensions"); }

MacAutostartView DecideMacAutostartView(bool agentExists, const QString &agentTarget, const QString &thisApp,
                                        MacLoginItemStatus status) {
    MacAutostartView v;
    if (!agentExists) return v;

    if (QDir::cleanPath(agentTarget) != QDir::cleanPath(thisApp)) {
        v.note = agentTarget.isEmpty()
            ? QStringLiteral("Start with system is set, but its login agent is not valid. Turn it on here to repair it.")
            : QStringLiteral("Start with system is set for another copy of Proxor (%1). Turn it on here to start this "
                             "copy instead.").arg(agentTarget);
        return v;
    }

    v.checked = true;
    if (status == MacLoginItemStatus::RequiresApproval) {
        v.needsAttention = true;
        v.note = QStringLiteral("Start with system is turned off in %1.").arg(MacLoginItemsLocation());
    } else if (status == MacLoginItemStatus::NotRegistered || status == MacLoginItemStatus::NotFound) {
        v.needsAttention = true;
        v.note = QStringLiteral("macOS does not list the Proxor login item yet.");
    }
    return v;
}

} // namespace ProxorPlatform

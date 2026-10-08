#include "platform/LinuxSystemProxyPlan.hpp"

#include <QFile>
#include <QHash>
#include <QSysInfo>
#include <QTemporaryDir>
#include <QtTest>

using namespace ProxorPlatform;

namespace {

LinuxDesktopInfo Kde(int major) {
    LinuxDesktopInfo d;
    d.family = LinuxDesktopFamily::Kde;
    d.kdeMajor = major;
    d.label = major ? QString("KDE Plasma %1").arg(major) : QString("KDE Plasma");
    return d;
}

LinuxDesktopInfo OfFamily(LinuxDesktopFamily f, bool gnomeSettings = false) {
    LinuxDesktopInfo d;
    d.family = f;
    d.usesGnomeProxySettings = gnomeSettings;
    d.label = LinuxDesktopFamilyName(f);
    return d;
}

ProgramLookup Lookup(const QStringList &present) {
    return [present](const QString &name) { return present.contains(name) ? "/usr/bin/" + name : QString(); };
}

QStringList Cmd(const LinuxProxyPlan &p) {
    QStringList l;
    for (const auto &a: p.actions) l << a.program + " " + a.arguments.join(' ');
    return l;
}

} // namespace

class LinuxSystemProxyTest : public QObject {
    Q_OBJECT

    static bool WriteScript(const QString &path, const QString &body) {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
        f.write(("#!/bin/sh\n" + body + "\n").toUtf8());
        f.close();
        return f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    }

private slots:
    void kdeToolChoice() {
        const auto both = Lookup({"kwriteconfig6", "kwriteconfig5"});
        QCOMPARE(FindKdeConfigTool(Kde(6), both), QString("/usr/bin/kwriteconfig6"));
        QCOMPARE(FindKdeConfigTool(Kde(5), both), QString("/usr/bin/kwriteconfig5"));
        QCOMPARE(FindKdeConfigTool(Kde(0), Lookup({"kwriteconfig5"})), QString("/usr/bin/kwriteconfig5"));
        QCOMPARE(FindKdeConfigTool(Kde(6), Lookup({"kwriteconfig5"})), QString("/usr/bin/kwriteconfig5"));
        QCOMPARE(FindKdeConfigTool(Kde(6), Lookup({})), QString());
    }
    void kdePlasma6Clear() {
        const auto plan = PlanLinuxProxyClear(Kde(6), Lookup({"kwriteconfig6"}), "/home/u/.config");
        QVERIFY(plan.unsupportedReason.isEmpty());
        const auto cmds = Cmd(plan);
        QVERIFY(cmds.contains("/usr/bin/kwriteconfig6 --file /home/u/.config/kioslaverc --group Proxy Settings --key ProxyType 0"));
        QCOMPARE(plan.actions.first().arguments.at(3), QString("Proxy Settings"));
        QVERIFY(plan.actions.first().required);
        bool dbus = false;
        for (const auto &a: plan.actions) {
            if (a.program == "dbus-send") {
                dbus = true;
                QVERIFY(!a.required);
            }
        }
        QVERIFY(dbus);
        QVERIFY(!cmds.join('\n').contains("kwriteconfig5"));
    }
    void kdeClearWithoutTool() {
        const auto plan = PlanLinuxProxyClear(Kde(6), Lookup({}), "/c");
        QVERIFY(plan.actions.isEmpty());
        QVERIFY(plan.unsupportedReason.contains("kwriteconfig6"));
        QVERIFY(plan.unsupportedReason.contains("kwriteconfig5"));
    }
    void gnomeClear() {
        const auto plan = PlanLinuxProxyClear(OfFamily(LinuxDesktopFamily::Gnome, true), Lookup({}), "/c");
        QCOMPARE(plan.actions.size(), 1);
        QCOMPARE(Cmd(plan).first(), QString("gsettings set org.gnome.system.proxy mode none"));
        QVERIFY(plan.actions.first().required);
    }
    void otherClearIsBestEffort() {
        for (auto f: {LinuxDesktopFamily::Unknown, LinuxDesktopFamily::Other, LinuxDesktopFamily::Xfce}) {
            const auto plan = PlanLinuxProxyClear(OfFamily(f), Lookup({}), "/c");
            QCOMPARE(plan.actions.size(), 1);
            QCOMPARE(plan.actions.first().program, QString("gsettings"));
            QVERIFY(!plan.actions.first().required);
        }
    }
    void kdeSet() {
        const auto lookup = Lookup({"kwriteconfig6"});
        const auto set = PlanLinuxProxySet(Kde(6), lookup, "/c", "127.0.0.1", 2080, 2081);
        QVERIFY(set.unsupportedReason.isEmpty());
        const auto cmds = Cmd(set).join('\n');
        const QString base = "/usr/bin/kwriteconfig6 --file /c/kioslaverc --group Proxy Settings --key ";
        for (const auto &k: {"httpProxy", "ftpProxy", "httpsProxy"})
            QVERIFY2(cmds.contains(base + k + " http://127.0.0.1 2080"), k);
        QVERIFY(cmds.contains(base + "socksProxy socks://127.0.0.1 2081"));
        QVERIFY(cmds.contains(base + "ProxyType 1"));
        QVERIFY(cmds.contains("/KIO/Scheduler"));
        // Set and Clear use the same tool.
        const auto clear = PlanLinuxProxyClear(Kde(6), lookup, "/c");
        QCOMPARE(set.actions.first().program, clear.actions.first().program);
    }
    void gnomeSet() {
        const auto set = PlanLinuxProxySet(OfFamily(LinuxDesktopFamily::Gnome, true), Lookup({}), "/c", "127.0.0.1", 2080, 2081);
        const QStringList expected{
            "gsettings set org.gnome.system.proxy.http host 127.0.0.1",   "gsettings set org.gnome.system.proxy.http port 2080",
            "gsettings set org.gnome.system.proxy.ftp host 127.0.0.1",    "gsettings set org.gnome.system.proxy.ftp port 2080",
            "gsettings set org.gnome.system.proxy.https host 127.0.0.1",  "gsettings set org.gnome.system.proxy.https port 2080",
            "gsettings set org.gnome.system.proxy.socks host 127.0.0.1",  "gsettings set org.gnome.system.proxy.socks port 2081",
            "gsettings set org.gnome.system.proxy mode manual"};
        QCOMPARE(Cmd(set), expected);
    }
    void unsupportedSet() {
        for (auto f: {LinuxDesktopFamily::Xfce, LinuxDesktopFamily::Other, LinuxDesktopFamily::Unknown}) {
            const auto set = PlanLinuxProxySet(OfFamily(f), Lookup({}), "/c", "127.0.0.1", 1, 2);
            QVERIFY(set.actions.isEmpty());
            QVERIFY(!set.unsupportedReason.isEmpty());
        }
    }
    void runnerFakeKwriteconfig6() {
        if (QSysInfo::kernelType() == "winnt") QSKIP("sh scripts");
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto log = dir.filePath("log");
        const auto tool = dir.filePath("kwriteconfig6");
        QVERIFY(WriteScript(tool, "echo \"$@\" >> '" + log + "'"));
        const auto lookup = [&](const QString &n) { return n == "kwriteconfig6" ? tool : QString(); };
        auto plan = PlanLinuxProxyClear(Kde(6), lookup, "/c");
        plan.actions.removeIf([](const ProxyAction &a) { return a.program == "dbus-send"; });
        const auto res = RunLinuxProxyActions(plan, RunProgramBlocking);
        QVERIFY(res.ok);
        QFile f(log);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QVERIFY(QString::fromUtf8(f.readAll()).contains("--key ProxyType 0"));
    }
    void runnerFailingTool() {
        if (QSysInfo::kernelType() == "winnt") QSKIP("sh scripts");
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto tool = dir.filePath("kwriteconfig6");
        QVERIFY(WriteScript(tool, "exit 1"));
        LinuxProxyPlan plan;
        plan.actions.append({tool, {"--key", "ProxyType", "0"}, true});
        const auto res = RunLinuxProxyActions(plan, RunProgramBlocking);
        QVERIFY(!res.ok);
        QCOMPARE(res.failures.size(), 1);
        QVERIFY(res.failures.first().contains("kwriteconfig6"));
        QVERIFY(res.failures.first().contains("exit 1"));
    }
    void runnerMissingProgram() {
        LinuxProxyPlan plan;
        plan.actions.append({"kwriteconfig6", {"x"}, true});
        const auto res = RunLinuxProxyActions(plan, [](const QString &, const QStringList &) { return -2; });
        QVERIFY(!res.ok);
        QVERIFY(res.failures.first().contains("not found"));
    }
    void runnerOptionalFailureIsOk() {
        LinuxProxyPlan plan;
        plan.actions.append({"dbus-send", {}, false});
        QVERIFY(RunLinuxProxyActions(plan, [](const QString &, const QStringList &) { return 1; }).ok);
    }
    void problemSink() {
        ReportSystemProxyProblem("x");
        QCOMPARE(TakeSystemProxyProblem(), QString("x"));
        QCOMPARE(TakeSystemProxyProblem(), QString());
    }
};

QTEST_APPLESS_MAIN(LinuxSystemProxyTest)
#include "linux_system_proxy_test.moc"

#include "platform/ProcessNames.hpp"

#include <QDir>
#include <QFile>
#include <QSysInfo>
#include <QTemporaryDir>
#include <QtTest>

using namespace ProxorPlatform;

class ProcessNamesTest : public QObject {
    Q_OBJECT
private:
    static void write(const QString &path, const QByteArray &data) {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(data);
    }
private slots:
    void resolve_data() {
        QTest::addColumn<QString>("exe");
        QTest::addColumn<QString>("argv0");
        QTest::addColumn<QString>("comm");
        QTest::addColumn<QString>("want");
        QTest::newRow("exe wins") << "/usr/bin/telegram-desktop" << "" << "telegram-deskto" << "telegram-desktop";
        QTest::newRow("deleted") << "/usr/lib/firefox/firefox (deleted)" << "" << "firefox" << "firefox";
        QTest::newRow("kernel thread") << "" << "" << "" << "";
        QTest::newRow("kworker") << "" << "" << "kworker/0:1" << "";
        QTest::newRow("short comm") << "" << "/usr/bin/python3" << "python3" << "python3";
        QTest::newRow("14 chars") << "" << "/opt/Signal/signal-desktop" << "signal-desktop" << "signal-desktop";
        QTest::newRow("argv0 extends") << "" << "/usr/bin/gnome-terminal-server" << "gnome-terminal-" << "gnome-terminal-server";
        QTest::newRow("truncated guess") << "" << "bash" << "some-long-name-x" << "";
    }
    void resolve() {
        QFETCH(QString, exe);
        QFETCH(QString, argv0);
        QFETCH(QString, comm);
        QFETCH(QString, want);
        QCOMPARE(ResolveLinuxProcessName(exe, argv0, comm), want);
    }

    void tree() {
        if (QSysInfo::kernelType() == QLatin1String("winnt"))
            QSKIP("QFile::link creates .lnk shortcuts on Windows");
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString r = tmp.path();
        auto mk = [&](const QString &pid, const QString &exe, const QByteArray &cmd, const QByteArray &comm) {
            QVERIFY(QDir().mkpath(r + "/" + pid));
            if (!exe.isEmpty()) QVERIFY(QFile::link(exe, r + "/" + pid + "/exe"));
            write(r + "/" + pid + "/cmdline", cmd);
            write(r + "/" + pid + "/comm", comm);
        };
        mk("1", "/usr/lib/systemd/systemd", "", "systemd\n");
        mk("42", "", QByteArray("/usr/bin/gnome-terminal-server\0--x\0", 35), "gnome-terminal-\n");
        mk("7", "", "", "kworker/0:1\n");
        mk("99", "/usr/bin/telegram-desktop", "", "telegram-deskto\n");
        mk("100", "/usr/bin/telegram-desktop", "", "telegram-deskto\n");
        mk("300", "", "x", "abcdefghijklmno\n");
        QVERIFY(QDir().mkpath(r + "/sys"));
        QVERIFY(QDir().mkpath(r + "/1x"));
        QVERIFY(QFile::link(r + "/1", r + "/self"));

        const auto list = ListLinuxProcessNames(r);
        QCOMPARE(list.names, (QStringList{"gnome-terminal-server", "systemd", "telegram-desktop"}));
        QCOMPARE(list.skipped, 1);
    }

    void missingRoot() {
        const auto list = ListLinuxProcessNames("/nonexistent-proxor-proc-root");
        QVERIFY(list.names.isEmpty());
        QCOMPARE(list.skipped, 0);
    }
};

QTEST_APPLESS_MAIN(ProcessNamesTest)
#include "process_names_test.moc"

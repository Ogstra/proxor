#include "LinuxCap.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QProcess>

#define EXIT_CODE(p) (p.exitStatus() == QProcess::NormalExit ? p.exitCode() : -1)

namespace {
QString FindTrustedSystemExecutable(const QString &name) {
    for (const auto &dir : {QStringLiteral("/usr/sbin"), QStringLiteral("/usr/bin"), QStringLiteral("/sbin"), QStringLiteral("/bin")}) {
        const QFileInfo candidate(QDir(dir).filePath(name));
        if (candidate.isFile() && candidate.isExecutable()) return candidate.absoluteFilePath();
    }
    return {};
}
}

QString Linux_GetCapString(const QString &path) {
    const auto getcap = Linux_FindCapProgsExec("getcap");
    if (getcap.isEmpty()) return {};
    QProcess p;
    p.setProgram(getcap);
    p.setArguments({path});
    p.start();
    p.waitForFinished(500);
    return p.readAllStandardOutput();
}

QString Linux_PkexecPath() {
    return FindTrustedSystemExecutable("pkexec");
}

int Linux_Pkexec_SetCapString(const QString &path, const QString &cap) {
    const auto pkexec = Linux_PkexecPath();
    const auto setcap = Linux_FindCapProgsExec("setcap");
    if (pkexec.isEmpty() || setcap.isEmpty()) return -1;
    QProcess p;
    p.setProgram(pkexec);
    p.setArguments({setcap, cap, path});
    p.start();
    p.waitForFinished(-1);
    return EXIT_CODE(p);
}

bool Linux_HavePkexec() {
    const auto pkexec = Linux_PkexecPath();
    if (pkexec.isEmpty()) return false;
    QProcess p;
    p.setProgram(pkexec);
    p.setArguments({"--help"});
    p.setProcessChannelMode(QProcess::SeparateChannels);
    p.start();
    p.waitForFinished(500);
    return EXIT_CODE(p) == 0;
}

bool Linux_HaveSetcap() {
    return !Linux_FindCapProgsExec("setcap").isEmpty();
}

QString Linux_FindCapProgsExec(const QString &name) {
    const auto exec = FindTrustedSystemExecutable(name);

    if (exec.isEmpty())
        qDebug() << "Executable" << name << "could not be resolved";
    else
        qDebug() << "Found exec" << name << "at" << exec;

    return exec;
}

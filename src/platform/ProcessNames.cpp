#include "ProcessNames.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>

#include <algorithm>

namespace ProxorPlatform {

namespace {
constexpr int kCommMax = 15; // the kernel truncates comm to 15 characters

QString BaseName(const QString &path) {
    return path.section(QLatin1Char('/'), -1);
}

QByteArray ReadSmall(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return f.read(4096);
}
} // namespace

QString ResolveLinuxProcessName(const QString &exeTarget, const QString &argv0, const QString &comm) {
    if (!exeTarget.isEmpty()) {
        QString t = exeTarget;
        const QString deleted = QStringLiteral(" (deleted)");
        if (t.endsWith(deleted)) t.chop(deleted.size());
        return BaseName(t);
    }
    if (argv0.isEmpty()) return {}; // kernel thread
    if (comm.size() < kCommMax) return comm;
    const QString base = BaseName(argv0);
    if (base.startsWith(comm)) return base;
    return {};
}

ProcessNameList ListLinuxProcessNames(const QString &procRoot) {
    ProcessNameList result;
    const QDir root(procRoot);
    if (!root.exists()) return result;

    QSet<QString> seen;
    const auto entries = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &e : entries) {
        if (e.isSymLink()) continue;
        const QString dirName = e.fileName();
        bool digits = !dirName.isEmpty();
        for (const QChar c : dirName)
            if (!c.isDigit()) { digits = false; break; }
        if (!digits) continue;

        const QString dir = e.absoluteFilePath();
        const QString exeTarget = QFileInfo(dir + QStringLiteral("/exe")).symLinkTarget();
        const QByteArray cmd = ReadSmall(dir + QStringLiteral("/cmdline"));
        const QString argv0 = QString::fromUtf8(cmd.left(cmd.indexOf('\0') < 0 ? cmd.size() : cmd.indexOf('\0')));
        const QString comm = QString::fromUtf8(ReadSmall(dir + QStringLiteral("/comm"))).trimmed();

        const QString name = ResolveLinuxProcessName(exeTarget, argv0, comm);
        if (name.isEmpty()) {
            if (exeTarget.isEmpty() && !argv0.isEmpty()) ++result.skipped;
            continue;
        }
        if (!seen.contains(name)) {
            seen.insert(name);
            result.names.append(name);
        }
    }
    std::sort(result.names.begin(), result.names.end(), [](const QString &a, const QString &b) {
        return QString::compare(a, b, Qt::CaseInsensitive) < 0;
    });
    return result;
}

} // namespace ProxorPlatform

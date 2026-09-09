#include "LogFile.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QRegularExpression>
#include <QTextStream>

namespace ProxorGui_log {

    namespace {
        // Ring of recent lines kept for the crash handler. Bounded so a chatty core cannot
        // grow it without limit.
        constexpr int kRecentMax = 400;

        QMutex g_mutex;
        QFile g_file;
        QStringList g_recent;
        QString g_openedForDate;
        bool g_initialised = false;
        int g_keepDays = 7;

        QString logDirPath() { return QDir::current().absoluteFilePath("logs"); }

        // Caller must hold g_mutex.
        void openForToday_locked() {
            const QString today = QDate::currentDate().toString("yyyy-MM-dd");
            if (g_file.isOpen() && g_openedForDate == today) return;

            if (g_file.isOpen()) g_file.close();

            QDir dir(logDirPath());
            if (!dir.exists() && !QDir().mkpath(dir.absolutePath())) return;

            g_file.setFileName(dir.absoluteFilePath("proxor-" + today + ".log"));
            if (!g_file.open(QIODevice::Append | QIODevice::Text)) return;
            g_openedForDate = today;
        }

        // Caller must hold g_mutex.
        void prune_locked() {
            QDir dir(logDirPath());
            if (!dir.exists()) return;
            const QDate cutoff = QDate::currentDate().addDays(-g_keepDays);
            const auto entries = dir.entryInfoList({"proxor-*.log"}, QDir::Files);
            for (const auto &fi : entries) {
                // proxor-YYYY-MM-DD.log
                const QString stamp = fi.completeBaseName().mid(QStringLiteral("proxor-").size());
                const QDate d = QDate::fromString(stamp, "yyyy-MM-dd");
                if (d.isValid() && d < cutoff) QFile::remove(fi.absoluteFilePath());
            }
        }
    } // namespace

    QString LevelName(Level level) {
        switch (level) {
            case Level::Debug:   return QStringLiteral("DEBUG");
            case Level::Warning: return QStringLiteral("WARN");
            case Level::Error:   return QStringLiteral("ERROR");
            case Level::Info:    break;
        }
        return QStringLiteral("INFO");
    }

    Level InferLevel(const QString &line) {
        const QString s = line.trimmed();
        if (s.startsWith(QStringLiteral("[Error]"), Qt::CaseInsensitive)) return Level::Error;
        if (s.startsWith(QStringLiteral("[Warning]"), Qt::CaseInsensitive)) return Level::Warning;
        if (s.startsWith(QStringLiteral("[Debug]"), Qt::CaseInsensitive)) return Level::Debug;
        return Level::Info;
    }

    void Init(int keepDays) {
        QMutexLocker lock(&g_mutex);
        g_keepDays = keepDays > 0 ? keepDays : 7;
        openForToday_locked();
        prune_locked();
        g_initialised = true;
    }

    void Shutdown() {
        QMutexLocker lock(&g_mutex);
        if (g_file.isOpen()) {
            g_file.flush();
            g_file.close();
        }
        g_initialised = false;
    }

    void Write(Level level, const QString &line) {
        QMutexLocker lock(&g_mutex);
        if (!g_initialised) return;

        // The core's output reaches us with VT100 colour escapes; they are meaningless in
        // a file and make it painful to grep.
        static const QRegularExpression ansiRe(QStringLiteral(R"(\x1B\[[0-9;]*[A-Za-z])"));
        const QString clean = QString(line).remove(ansiRe).trimmed();

        const QString stamped = QStringLiteral("%1 %2 %3")
                                    .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz"),
                                         LevelName(level).leftJustified(5),
                                         clean);

        g_recent.append(stamped);
        while (g_recent.size() > kRecentMax) g_recent.removeFirst();

        // Reopen across midnight so a long-running session does not keep writing into
        // yesterday's file.
        openForToday_locked();
        if (!g_file.isOpen()) return;

        QTextStream out(&g_file);
        out << stamped << '\n';
        out.flush();
    }

    QStringList Recent() {
        // Never block the crash path: the fault may have happened while the lock was held.
        if (!g_mutex.tryLock(200)) return {};
        QStringList copy = g_recent;
        g_mutex.unlock();
        return copy;
    }

} // namespace ProxorGui_log

#pragma once

#include <QString>
#include <QStringList>

// Disk sink for the application log.
//
// Until this existed, every MW_show_log line lived only in the in-app log widget, so
// whenever the process died the context that explained the crash died with it. Crash
// dumps arrived with no accompanying log at all.
namespace ProxorGui_log {

    enum class Level { Debug, Info, Warning, Error };

    QString LevelName(Level level);

    // Existing call sites encode severity in the message text ("[Error] ...", "[Warning] ...").
    // Rather than churn 45 call sites, infer from that convention; new code should pass a
    // Level explicitly to Write().
    Level InferLevel(const QString &line);

    // Opens <cwd>/logs/proxor-YYYY-MM-DD.log for append and prunes files older than
    // keepDays. Safe to call more than once. Failure is non-fatal and silent: logging must
    // never be the reason the app does not start.
    void Init(int keepDays = 7);
    void Shutdown();

    // Appends one timestamped, levelled line. Thread-safe.
    void Write(Level level, const QString &line);

    // The most recent lines, oldest first, for attaching to a crash dump.
    //
    // Called from the unhandled-exception filter, so it must never block: the crash may
    // have happened while the writer lock was held. Uses a bounded tryLock and returns
    // what it can.
    QStringList Recent();

} // namespace ProxorGui_log

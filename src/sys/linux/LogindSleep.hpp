#pragma once
// Linux sleep/wake events from systemd-logind (phase 56): org.freedesktop.login1.Manager.PrepareForSleep(bool).
#include <QDBusConnection>
#include <QObject>
#include <QString>

inline constexpr char kLogindService[] = "org.freedesktop.login1";
inline constexpr char kLogindPath[] = "/org/freedesktop/login1";
inline constexpr char kLogindManager[] = "org.freedesktop.login1.Manager";

class LogindSleepListener : public QObject {
    Q_OBJECT
public:
    explicit LogindSleepListener(const QDBusConnection &bus, QObject *parent = nullptr);
    bool start();                  // false if the bus is not connected or the match cannot be added; never blocks > 1 s
    bool serviceAvailable() const; // org.freedesktop.login1 currently has an owner (checked in start(), bounded)
    QString detail() const;        // one line for the log file
signals:
    void sleeping(bool goingToSleep); // PrepareForSleep(true) -> true, PrepareForSleep(false) -> false
private slots:
    void onPrepareForSleep(bool start);

private:
    QDBusConnection bus_;
    bool available_ = false;
    QString detail_;
};

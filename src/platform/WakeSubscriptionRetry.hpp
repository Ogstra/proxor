#pragma once

// Pure bounded backoff for the post-wake subscription update (Qt Core only).

#include <QString>
#include <QtGlobal>

namespace ProxorPlatform {

inline constexpr int kWakeSubscriptionRetryDelaysMs[] = {3000, 10000, 30000, 60000}; // after attempts 1..4
inline constexpr int kWakeSubscriptionMaxAttempts = 5;
inline constexpr int kWakeSubscriptionPollMs = 1000;
inline constexpr qint64 kWakeSubscriptionMaxMs = 600000; // whole retry, including waits for a pending start / running update

struct SubscriptionRetryInputs {
    qint64 nowMs = 0;
    bool blocked = false;       // startup Tun pending/failed, or exiting
    bool scheduled = false;     // UI_has_scheduled_subscription_updates()
    bool startPending = false;  // MainWindow::start_pending
    bool updateRunning = false; // UI_subscription_updates_running()
    bool anyDue = false;        // UI_has_due_subscription_updates()
};

enum class SubscriptionRetryAction { Wait, StartUpdate, Done, GaveUp };

struct SubscriptionRetryStep {
    SubscriptionRetryAction action = SubscriptionRetryAction::Done;
    int delayMs = -1; // >= 0: call step() again after this delay; -1: finished
    int attempt = 0;  // StartUpdate: 1-based attempt number; GaveUp: attempts made
};

class WakeSubscriptionRetry {
public:
    void begin(qint64 nowMs); // (re)starts: first attempt as soon as allowed
    SubscriptionRetryStep step(const SubscriptionRetryInputs &in);
    bool active() const;
    int attempts() const;
    void cancel();

private:
    bool active_ = false;
    bool inFlight_ = false;
    int attempts_ = 0;
    qint64 beganMs_ = 0;
    qint64 nextAtMs_ = 0;
};

// "Resumed from sleep: subscriptions could not be updated after %1 attempts; the regular schedule will try again."
QString WakeSubscriptionGaveUpLine(int attempts);

} // namespace ProxorPlatform

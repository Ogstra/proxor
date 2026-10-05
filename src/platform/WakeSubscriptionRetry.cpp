#include "WakeSubscriptionRetry.hpp"

#include <QCoreApplication>

namespace ProxorPlatform {

void WakeSubscriptionRetry::begin(qint64 nowMs) {
    active_ = true;
    inFlight_ = false;
    attempts_ = 0;
    beganMs_ = nowMs;
    nextAtMs_ = nowMs;
}

SubscriptionRetryStep WakeSubscriptionRetry::step(const SubscriptionRetryInputs &in) {
    if (!active_) return {SubscriptionRetryAction::Done, -1, attempts_};
    if (in.blocked || !in.scheduled) {
        active_ = false;
        return {SubscriptionRetryAction::Done, -1, attempts_};
    }
    if (in.nowMs - beganMs_ >= kWakeSubscriptionMaxMs) {
        active_ = false;
        return {SubscriptionRetryAction::GaveUp, -1, attempts_};
    }
    if (in.startPending || in.updateRunning) return {SubscriptionRetryAction::Wait, kWakeSubscriptionPollMs, attempts_};
    if (inFlight_) { // the attempt we started has ended
        inFlight_ = false;
        if (!in.anyDue) {
            active_ = false;
            return {SubscriptionRetryAction::Done, -1, attempts_};
        }
        if (attempts_ >= kWakeSubscriptionMaxAttempts) {
            active_ = false;
            return {SubscriptionRetryAction::GaveUp, -1, attempts_};
        }
        nextAtMs_ = in.nowMs + kWakeSubscriptionRetryDelaysMs[attempts_ - 1];
    }
    if (!in.anyDue) {
        active_ = false;
        return {SubscriptionRetryAction::Done, -1, attempts_};
    }
    if (in.nowMs < nextAtMs_) return {SubscriptionRetryAction::Wait, int(nextAtMs_ - in.nowMs), attempts_};
    ++attempts_;
    inFlight_ = true;
    return {SubscriptionRetryAction::StartUpdate, kWakeSubscriptionPollMs, attempts_};
}

bool WakeSubscriptionRetry::active() const { return active_; }
int WakeSubscriptionRetry::attempts() const { return attempts_; }
void WakeSubscriptionRetry::cancel() { active_ = false; }

QString WakeSubscriptionGaveUpLine(int attempts) {
    return QCoreApplication::translate("Wake", "Resumed from sleep: subscriptions could not be updated after %1 attempts; the regular schedule will try again.")
        .arg(attempts);
}

} // namespace ProxorPlatform

#include "WakeSubscriptionRetry.hpp"

namespace ProxorPlatform {

void WakeSubscriptionRetry::begin(qint64) {}

SubscriptionRetryStep WakeSubscriptionRetry::step(const SubscriptionRetryInputs &) {
    return {SubscriptionRetryAction::StartUpdate, kWakeSubscriptionPollMs, 1};
}

bool WakeSubscriptionRetry::active() const { return true; }
int WakeSubscriptionRetry::attempts() const { return 0; }
void WakeSubscriptionRetry::cancel() {}

QString WakeSubscriptionGaveUpLine(int) { return QString(); }

} // namespace ProxorPlatform

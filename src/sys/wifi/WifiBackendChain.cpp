#include "sys/wifi/WifiBackendChain.hpp"

#include <QElapsedTimer>
#include <algorithm>

using namespace ProxorWifi;

WifiReading ChooseWifiReading(const WifiReading &nm, NmWifiPresence nmPresence, const IwdRead &readIwd) {
    // NetworkManager answered about Wi-Fi (or broke while doing so): its reading stands, iwd is not asked.
    if (nmPresence == NmWifiPresence::ManagesWifi || nmPresence == NmWifiPresence::TimedOut || nmPresence == NmWifiPresence::Failed)
        return nm;
    const std::pair<WifiReading, IwdPresence> iwd = readIwd();
    if (iwd.second == IwdPresence::Present) return iwd.first;
    // iwd did not really answer: the NetworkManager reading is returned unchanged (no text of the chain's own).
    return nm;
}

NmThenIwdWifiReader::NmThenIwdWifiReader(std::unique_ptr<NetworkManagerWifiReader> nm, std::unique_ptr<IwdWifiReader> iwd, int totalBudgetMs)
    : nm_(std::move(nm)), iwd_(std::move(iwd)), totalBudgetMs_(totalBudgetMs) {}

WifiReading NmThenIwdWifiReader::read() {
    QElapsedTimer timer;
    timer.start();
    const WifiReading nmReading = nm_->read();
    const int remaining = totalBudgetMs_ - int(timer.elapsed());
    auto readIwd = [&]() -> std::pair<WifiReading, IwdPresence> {
        if (remaining < 200) return {Unavailable(QString(), QStringLiteral("iwd")), IwdPresence::Unreachable};
        const WifiReading r = iwd_->readWithin(std::min(remaining, 1500));
        return {r, iwd_->presence()};
    };
    return ChooseWifiReading(nmReading, nm_->presence(), readIwd);
}

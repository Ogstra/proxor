// Linux portal availability probe.
// Bounded stall (documented): the probe runs on a detached worker thread started by StartPortalProbe(), which main()
// calls before the window is built, so Portals() normally returns instantly (0 ms). The worst case a caller blocks
// is 1.5 s in total since StartPortalProbe(); with no session bus the probe finishes immediately.
#include "sys/DesktopPortal.hpp"
#include "XdgPortal.hpp"

#include <QDBusConnection>
#include <QFile>

#include <algorithm>
#include <chrono>
#include <future>
#include <mutex>
#include <thread>

namespace ProxorDesktop {

namespace {

constexpr int kProbeBudgetMs = 1500;

std::mutex g_mutex;
bool g_started = false;
std::chrono::steady_clock::time_point g_startedAt;
std::shared_future<PortalVersions> g_future;
bool g_haveCached = false;
PortalVersions g_cached;

PortalVersions RunProbe() {
    const QString name = QStringLiteral("proxor-portal-probe");
    PortalVersions v;
    {
        // Own named connection: no contention with the main thread's session bus.
        XdgPortalClient client(QDBusConnection::connectToBus(QDBusConnection::SessionBus, name));
        v = ProbePortalVersions(client, QFile::exists(QStringLiteral("/.flatpak-info")), QStringLiteral("proxor"), kProbeBudgetMs);
    }
    QDBusConnection::disconnectFromBus(name);
    return v;
}

// Caller holds g_mutex.
void StartLocked() {
    if (g_started) return;
    g_started = true;
    g_startedAt = std::chrono::steady_clock::now();
    auto promise = std::make_shared<std::promise<PortalVersions>>();
    g_future = promise->get_future().share();
    std::thread([promise] { promise->set_value(RunProbe()); }).detach();
}

} // namespace

void StartPortalProbe() {
    std::lock_guard<std::mutex> lock(g_mutex);
    StartLocked();
}

const PortalVersions &Portals() {
    static const PortalVersions running = [] {
        PortalVersions v;
        v.detail = QStringLiteral("portal probe still running");
        return v;
    }();
    std::shared_future<PortalVersions> future;
    std::chrono::milliseconds left;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_haveCached) return g_cached;
        StartLocked();
        future = g_future;
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - g_startedAt);
        left = std::max(std::chrono::milliseconds(0), std::chrono::milliseconds(kProbeBudgetMs) - elapsed);
    }
    if (future.wait_for(left) != std::future_status::ready) return running; // not cached: a later call gets the real answer
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_haveCached) {
        g_cached = future.get();
        g_haveCached = true;
    }
    return g_cached;
}

} // namespace ProxorDesktop

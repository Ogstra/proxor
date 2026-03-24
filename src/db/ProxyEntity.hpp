#pragma once

#include "main/ProxorGui.hpp"
#include "db/traffic/TrafficData.hpp"
#include "fmt/AbstractBean.hpp"

namespace ProxorGui_fmt {
    class SocksHttpBean;

    class ShadowSocksBean;

    class VMessBean;

    class TrojanVLESSBean;

    class NaiveBean;

    class QUICBean;

    class CustomBean;

    class ChainBean;
}; // namespace ProxorGui_fmt

namespace ProxorGui {
    class ProxyEntity : public JsonStore {
    public:
        QString type;

        int id = -1;
        int gid = 0;
        int latency = 0;
        std::shared_ptr<ProxorGui_fmt::AbstractBean> bean;
        std::shared_ptr<ProxorGui_traffic::TrafficData> traffic_data = std::make_shared<ProxorGui_traffic::TrafficData>("");

        QString full_test_report;

        ProxyEntity(ProxorGui_fmt::AbstractBean *bean, const QString &type_);

        [[nodiscard]] QString DisplayLatency() const;

        [[nodiscard]] QColor DisplayLatencyColor() const;

        [[nodiscard]] ProxorGui_fmt::ChainBean *ChainBean() const {
            return (ProxorGui_fmt::ChainBean *) bean.get();
        };

        [[nodiscard]] ProxorGui_fmt::SocksHttpBean *SocksHTTPBean() const {
            return (ProxorGui_fmt::SocksHttpBean *) bean.get();
        };

        [[nodiscard]] ProxorGui_fmt::ShadowSocksBean *ShadowSocksBean() const {
            return (ProxorGui_fmt::ShadowSocksBean *) bean.get();
        };

        [[nodiscard]] ProxorGui_fmt::VMessBean *VMessBean() const {
            return (ProxorGui_fmt::VMessBean *) bean.get();
        };

        [[nodiscard]] ProxorGui_fmt::TrojanVLESSBean *TrojanVLESSBean() const {
            return (ProxorGui_fmt::TrojanVLESSBean *) bean.get();
        };

        [[nodiscard]] ProxorGui_fmt::NaiveBean *NaiveBean() const {
            return (ProxorGui_fmt::NaiveBean *) bean.get();
        };

        [[nodiscard]] ProxorGui_fmt::QUICBean *QUICBean() const {
            return (ProxorGui_fmt::QUICBean *) bean.get();
        };

        [[nodiscard]] ProxorGui_fmt::CustomBean *CustomBean() const {
            return (ProxorGui_fmt::CustomBean *) bean.get();
        };
    };
} // namespace ProxorGui

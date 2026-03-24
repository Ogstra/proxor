#pragma once

#include "fmt/AbstractBean.hpp"
#include "fmt/V2RayStreamSettings.hpp"

namespace ProxorGui_fmt {
    class ShadowSocksBean : public AbstractBean {
    public:
        QString method = "aes-128-gcm";
        QString password = "";
        QString plugin = "";
        int uot = 0;

        std::shared_ptr<V2rayStreamSettings> stream = std::make_shared<V2rayStreamSettings>();

        ShadowSocksBean() : AbstractBean(0) {
            _add(new configItem("method", &method, itemType::string));
            _add(new configItem("pass", &password, itemType::string));
            _add(new configItem("plugin", &plugin, itemType::string));
            _add(new configItem("uot", &uot, itemType::integer));
            _add(new configItem("stream", dynamic_cast<JsonStore *>(stream.get()), itemType::jsonStore));
        };

        QString DisplayType() override { return "Shadowsocks"; };

        [[nodiscard]] QString PluginName() const {
            return SubStrBefore(plugin, ";").trimmed();
        }

        [[nodiscard]] QString PluginOptions() const {
            const auto name = PluginName();
            if (name.isEmpty()) return {};
            return SubStrAfter(plugin, ";").trimmed();
        }

        void SetPluginParts(const QString &name, const QString &options) {
            plugin = name.trimmed();
            const auto trimmedOptions = options.trimmed();
            if (!plugin.isEmpty() && !trimmedOptions.isEmpty()) {
                plugin += ";" + trimmedOptions;
            }
        }

        CoreObjOutboundBuildResult BuildCoreObjSingBox() override;

        bool TryParseLink(const QString &link);

        QString ToShareLink() override;
    };
} // namespace ProxorGui_fmt

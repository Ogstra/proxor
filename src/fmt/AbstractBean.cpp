#include "includes.h"

#include <QApplication>
#include <QHostInfo>
#include <QUrl>

namespace ProxorGui_fmt {
    AbstractBean::AbstractBean(int version) {
        this->version = version;
        _add(new configItem("_v", &this->version, itemType::integer));
        _add(new configItem("name", &name, itemType::string));
        _add(new configItem("addr", &serverAddress, itemType::string));
        _add(new configItem("port", &serverPort, itemType::integer));
        _add(new configItem("c_cfg", &custom_config, itemType::string));
        _add(new configItem("c_out", &custom_outbound, itemType::string));
    }

    QString AbstractBean::ToProxorShareLink(const QString &type) {
        auto b = ToJson();
        QUrl url;
        url.setScheme("proxor");
        url.setHost(type);
        url.setFragment(QJsonObject2QString(b, true)
                            .toUtf8()
                            .toBase64(QByteArray::Base64UrlEncoding));
        return url.toString();
    }

    QString AbstractBean::DisplayAddress() {
        return ::DisplayAddress(serverAddress, serverPort);
    }

    QString AbstractBean::DisplayName() {
        if (name.isEmpty()) {
            return DisplayAddress();
        }
        return name;
    }

    QString AbstractBean::DisplayTypeAndName() {
        return QStringLiteral("[%1] %2").arg(DisplayType(), DisplayName());
    }

    QString AbstractBean::MetadataStrippedIdentity() {
        return QJsonObject2QString(ToJson({"c_cfg", "c_out", "name"}), true) + DisplayType();
    }

    bool AbstractBean::CanResolveDomainToIP() const {
        if (dynamic_cast<const ChainBean *>(this) != nullptr) return false;
        if (dynamic_cast<const CustomBean *>(this) != nullptr) return false;
        if (dynamic_cast<const NaiveBean *>(this) != nullptr) return false;
        if (IsIpAddress(serverAddress)) return false;
        return true;
    }

    void AbstractBean::ResolveDomainToIP(const std::function<void(bool)> &onFinished) {
        if (!CanResolveDomainToIP()) {
            onFinished(false);
            return;
        }

#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0) // TODO older QT
        QHostInfo::lookupHost(serverAddress, QApplication::instance(), [=](const QHostInfo &host) {
            auto addr = host.addresses();
            bool resolved = false;
            if (!addr.isEmpty()) {
                auto domain = serverAddress;
                auto stream = GetStreamSettings(this);

                // replace serverAddress
                serverAddress = addr.first().toString();
                resolved = true;

                // replace ws tls
                if (stream != nullptr) {
                    if (stream->security == "tls" && stream->sni.isEmpty()) {
                        stream->sni = domain;
                    }
                    if (stream->network == "ws" && stream->host.isEmpty()) {
                        stream->host = domain;
                    }
                }
            }
            onFinished(resolved);
        });
#else
        onFinished(false);
#endif
    }
} // namespace ProxorGui_fmt

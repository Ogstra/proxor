#include "HTTPRequestHelper.hpp"

#include <QByteArray>
#include <QNetworkProxy>
#include <QEventLoop>
#include <QMetaEnum>
#include <QTimer>

#include "main/ProxorGui.hpp"

namespace ProxorGui_network {

    ProxorHttpResponse NetworkRequestHelper::HttpGet(const QUrl &url) {
        QNetworkRequest request;
        QNetworkAccessManager accessManager;
        request.setUrl(url);
        // Set proxy
        if (ProxorGui::dataStore->sub_use_proxy) {
            QNetworkProxy p;
            // Note: sing-box mixed socks5 protocol error
            p.setType(QNetworkProxy::HttpProxy);
            p.setHostName("127.0.0.1");
            p.setPort(ProxorGui::dataStore->inbound_socks_port);
            if (ProxorGui::dataStore->inbound_auth->NeedAuth()) {
                p.setUser(ProxorGui::dataStore->inbound_auth->username);
                p.setPassword(ProxorGui::dataStore->inbound_auth->password);
            }
            accessManager.setProxy(p);
            if (ProxorGui::dataStore->started_id < 0) {
                return ProxorHttpResponse{QObject::tr("Request with proxy but no profile started.")};
            }
        }
        if (accessManager.proxy().type() == QNetworkProxy::Socks5Proxy) {
            auto cap = accessManager.proxy().capabilities();
            accessManager.proxy().setCapabilities(cap | QNetworkProxy::HostNameLookupCapability);
        }
        // Set attribute
#if (QT_VERSION >= QT_VERSION_CHECK(5, 9, 0))
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
#endif
        request.setHeader(QNetworkRequest::KnownHeaders::UserAgentHeader, ProxorGui::dataStore->GetUserAgent());

        // X-* headers for server-side tracking (conditional on datastore toggles)
        auto setTruncated = [&](const QByteArray &name, const QString &value, int maxLen) {
            if (!value.isEmpty()) {
                request.setRawHeader(name, value.left(maxLen).toUtf8());
            }
        };
        setTruncated("X-Hwid", ProxorGui::dataStore->GetHwid(), 255);
        setTruncated("X-Device-Model", ProxorGui::dataStore->GetDeviceModel(), 255);
        setTruncated("X-Device-OS", ProxorGui::dataStore->GetDeviceOS(), 64);
        setTruncated("X-Ver-Os", ProxorGui::dataStore->GetOSVersion(), 64);
        setTruncated("X-App-Version", ProxorGui::dataStore->GetAppVersion(), 64);

        if (ProxorGui::dataStore->sub_insecure) {
            QSslConfiguration c;
            c.setPeerVerifyMode(QSslSocket::PeerVerifyMode::VerifyNone);
            request.setSslConfiguration(c);
        }
        //
        auto _reply = accessManager.get(request);
        connect(_reply, &QNetworkReply::sslErrors, _reply, [](const QList<QSslError> &errors) {
            QStringList error_str;
            for (const auto &err: errors) {
                error_str << err.errorString();
            }
            MW_show_log(QStringLiteral("SSL Errors: %1 %2").arg(error_str.join(","), ProxorGui::dataStore->sub_insecure ? "(Ignored)" : ""));
        });
        // Wait for response
        QTimer abortTimer;
        abortTimer.setSingleShot(true);
        abortTimer.setInterval(10000);
        QObject::connect(&abortTimer, &QTimer::timeout, _reply, &QNetworkReply::abort);
        abortTimer.start();
        {
            QEventLoop loop;
            QObject::connect(_reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            loop.exec();
        }
        abortTimer.stop();
        //
        auto result = ProxorHttpResponse{_reply->error() == QNetworkReply::NetworkError::NoError ? "" : _reply->errorString(),
                                       _reply->readAll(), _reply->rawHeaderPairs()};
        _reply->deleteLater();
        return result;
    }

    QString NetworkRequestHelper::GetHeader(const QList<QPair<QByteArray, QByteArray>> &header, const QString &name) {
        for (const auto &p: header) {
            if (QString(p.first).toLower() == name.toLower()) return p.second;
        }
        return "";
    }

} // namespace ProxorGui_network

#pragma once

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <functional>

namespace ProxorGui_network {
    struct ProxorHttpResponse {
        QString error;
        QByteArray data;
        QList<QPair<QByteArray, QByteArray>> header;
    };

    class NetworkRequestHelper : QObject {
        Q_OBJECT

        explicit NetworkRequestHelper(QObject *parent) : QObject(parent){};

        ~NetworkRequestHelper() override = default;
        ;

    public:
        static ProxorHttpResponse HttpGet(const QUrl &url);

        static QString GetHeader(const QList<QPair<QByteArray, QByteArray>> &header, const QString &name);
    };
} // namespace ProxorGui_network

using namespace ProxorGui_network;

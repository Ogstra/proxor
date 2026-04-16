#include "db/ProfileFilter.hpp"
#include "fmt/includes.h"
#include "fmt/Preset.hpp"
#include "main/HTTPRequestHelper.hpp"

#include "GroupUpdater.hpp"

#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUrlQuery>

#ifndef NKR_NO_YAML

#include <yaml-cpp/yaml.h>

#endif

namespace ProxorGui_sub {

    GroupUpdater *groupUpdater = new GroupUpdater;

    namespace {
        QString JsonString(const QJsonObject &obj, const std::initializer_list<const char *> &keys) {
            for (const auto *key: keys) {
                const auto value = obj.value(key);
                if (value.isString()) return value.toString();
            }
            return {};
        }

        int JsonInt(const QJsonObject &obj, const std::initializer_list<const char *> &keys, int def = 0) {
            for (const auto *key: keys) {
                const auto value = obj.value(key);
                if (value.isDouble()) return value.toInt(def);
                if (value.isString()) return value.toString().toInt();
            }
            return def;
        }

        QString DecodeSubscriptionTitle(QString title) {
            title = title.trimmed();
            if (title.startsWith("base64:", Qt::CaseInsensitive)) {
                const auto decoded = QByteArray::fromBase64(title.mid(7).toUtf8());
                if (!decoded.isEmpty()) return QString::fromUtf8(decoded).trimmed();
            }
            return title;
        }

        QString NameFromContentDisposition(const QString &contentDisposition) {
            if (contentDisposition.isEmpty()) return {};

            const QRegularExpression utf8Pattern(R"(filename\*\s*=\s*UTF-8''([^;]+))",
                                                 QRegularExpression::CaseInsensitiveOption);
            auto match = utf8Pattern.match(contentDisposition);
            if (match.hasMatch()) {
                return QUrl::fromPercentEncoding(match.captured(1).toUtf8()).trimmed();
            }

            const QRegularExpression plainPattern(R"(filename\s*=\s*\"?([^\";]+)\"?)",
                                                  QRegularExpression::CaseInsensitiveOption);
            match = plainPattern.match(contentDisposition);
            if (match.hasMatch()) {
                return match.captured(1).trimmed();
            }

            return {};
        }

        QString NameFromSubscriptionUrl(const QUrl &url) {
            if (!url.isValid()) return {};
            return url.fragment(QUrl::FullyDecoded).trimmed();
        }

        QString ResolveSubscriptionName(const QUrl &url, const QList<QPair<QByteArray, QByteArray>> &headers) {
            auto profileTitle = DecodeSubscriptionTitle(NetworkRequestHelper::GetHeader(headers, "profile-title"));
            if (!profileTitle.isEmpty()) return profileTitle;

            auto contentDisposition = NameFromContentDisposition(
                NetworkRequestHelper::GetHeader(headers, "content-disposition"));
            if (!contentDisposition.isEmpty()) return contentDisposition;

            return NameFromSubscriptionUrl(url);
        }

        bool UpdateSip008(const QString &str, RawUpdater *updater) {
            QJsonParseError error;
            const auto doc = QJsonDocument::fromJson(str.toUtf8(), &error);
            if (error.error != QJsonParseError::NoError || !doc.isObject()) return false;

            const auto root = doc.object();
            const auto servers = root.value("servers");
            if (!servers.isArray()) return false;

            for (const auto &value: servers.toArray()) {
                if (!value.isObject()) continue;

                const auto server = value.toObject();
                if (server.value("disabled").toBool(false)) continue;

                const auto address = JsonString(server, {"server", "address"});
                const auto port = JsonInt(server, {"server_port", "port"});
                const auto method = JsonString(server, {"method", "cipher"});
                const auto password = JsonString(server, {"password"});
                if (address.isEmpty() || port <= 0 || method.isEmpty() || password.isEmpty()) continue;

                auto ent = ProxorGui::ProfileManager::NewProxyEntity("shadowsocks");
                auto bean = ent->ShadowSocksBean();

                ent->bean->name = JsonString(server, {"remarks", "name", "tag"});
                ent->bean->serverAddress = address;
                ent->bean->serverPort = port;
                bean->method = method;
                bean->password = password;

                const auto plugin = JsonString(server, {"plugin"});
                const auto pluginOpts = JsonString(server, {"plugin_opts", "plugin-opts"});
                if (!plugin.isEmpty()) {
                    bean->SetPluginParts(plugin, pluginOpts);
                }

                ProxorGui::profileManager->AddProfile(ent, updater->gid_add_to);
                updater->updated_order += ent;
            }

            return true;
        }
    } // namespace

    void RawUpdater_FixEnt(const std::shared_ptr<ProxorGui::ProxyEntity> &ent) {
        if (ent == nullptr) return;
        auto stream = ProxorGui_fmt::GetStreamSettings(ent->bean.get());
        if (stream == nullptr) return;
        // 1. "security"
        if (stream->security == "none" || stream->security == "0" || stream->security == "false") {
            stream->security = "";
        } else if (stream->security == "1" || stream->security == "true") {
            stream->security = "tls";
        }
        // 2. TLS SNI: v2rayN config builder generate sni like this, so set sni here for their format.
        if (stream->security == "tls" && IsIpAddress(ent->bean->serverAddress) && (!stream->host.isEmpty()) && stream->sni.isEmpty()) {
            stream->sni = stream->host;
        }
    }

    void RawUpdater::update(const QString &str) {
        // Base64 encoded subscription
        if (auto str2 = DecodeB64IfValid(str); !str2.isEmpty()) {
            update(str2);
            return;
        }

        // Clash
        if (str.contains("proxies:")) {
            updateClash(str);
            return;
        }

        // SIP008 / multi-server Shadowsocks JSON
        if (UpdateSip008(str, this)) {
            return;
        }

        // Multi line
        if (str.count("\n") > 0) {
            auto list = str.split("\n");
            for (const auto &str2: list) {
                update(str2.trimmed());
            }
            return;
        }

        std::shared_ptr<ProxorGui::ProxyEntity> ent;
        bool needFix = true;

        // Proxor format
        if (str.startsWith("proxor://")) {
            needFix = false;
            auto link = QUrl(str);
            if (!link.isValid()) return;
            ent = ProxorGui::ProfileManager::NewProxyEntity(link.host());
            if (ent->bean->version == -114514) return;
            auto j = DecodeB64IfValid(link.fragment().toUtf8(), QByteArray::Base64UrlEncoding);
            if (j.isEmpty()) return;
            ent->bean->FromJsonBytes(j);
        }

        // SOCKS
        if (str.startsWith("socks5://") || str.startsWith("socks4://") ||
            str.startsWith("socks4a://") || str.startsWith("socks://")) {
            ent = ProxorGui::ProfileManager::NewProxyEntity("socks");
            auto ok = ent->SocksHTTPBean()->TryParseLink(str);
            if (!ok) return;
        }

        // HTTP
        if (str.startsWith("http://") || str.startsWith("https://")) {
            ent = ProxorGui::ProfileManager::NewProxyEntity("http");
            auto ok = ent->SocksHTTPBean()->TryParseLink(str);
            if (!ok) return;
        }

        // ShadowSocks
        if (str.startsWith("ss://")) {
            ent = ProxorGui::ProfileManager::NewProxyEntity("shadowsocks");
            auto ok = ent->ShadowSocksBean()->TryParseLink(str);
            if (!ok) return;
        }

        // VMess
        if (str.startsWith("vmess://")) {
            ent = ProxorGui::ProfileManager::NewProxyEntity("vmess");
            auto ok = ent->VMessBean()->TryParseLink(str);
            if (!ok) return;
        }

        // VLESS
        if (str.startsWith("vless://")) {
            ent = ProxorGui::ProfileManager::NewProxyEntity("vless");
            auto ok = ent->TrojanVLESSBean()->TryParseLink(str);
            if (!ok) return;
        }

        // Trojan
        if (str.startsWith("trojan://")) {
            ent = ProxorGui::ProfileManager::NewProxyEntity("trojan");
            auto ok = ent->TrojanVLESSBean()->TryParseLink(str);
            if (!ok) return;
        }

        // Naive
        if (str.startsWith("naive+")) {
            needFix = false;
            ent = ProxorGui::ProfileManager::NewProxyEntity("naive");
            auto ok = ent->NaiveBean()->TryParseLink(str);
            if (!ok) return;
        }

        // Hysteria2
        if (str.startsWith("hysteria2://") || str.startsWith("hy2://")) {
            needFix = false;
            ent = ProxorGui::ProfileManager::NewProxyEntity("hysteria2");
            auto ok = ent->QUICBean()->TryParseLink(str);
            if (!ok) return;
        }

        // TUIC
        if (str.startsWith("tuic://")) {
            needFix = false;
            ent = ProxorGui::ProfileManager::NewProxyEntity("tuic");
            auto ok = ent->QUICBean()->TryParseLink(str);
            if (!ok) return;
        }

        if (ent == nullptr) return;

        // Fix
        if (needFix) RawUpdater_FixEnt(ent);

        // End
        ProxorGui::profileManager->AddProfile(ent, gid_add_to);
        updated_order += ent;
    }

#ifndef NKR_NO_YAML

    QString Node2QString(const YAML::Node &n, const QString &def = "") {
        try {
            return n.as<std::string>().c_str();
        } catch (const YAML::Exception &ex) {
            qDebug() << ex.what();
            return def;
        }
    }

    QStringList Node2QStringList(const YAML::Node &n) {
        try {
            if (n.IsSequence()) {
                QStringList list;
                for (auto item: n) {
                    list << item.as<std::string>().c_str();
                }
                return list;
            } else {
                return {};
            }
        } catch (const YAML::Exception &ex) {
            qDebug() << ex.what();
            return {};
        }
    }

    int Node2Int(const YAML::Node &n, const int &def = 0) {
        try {
            return n.as<int>();
        } catch (const YAML::Exception &ex) {
            qDebug() << ex.what();
            return def;
        }
    }

    bool Node2Bool(const YAML::Node &n, const bool &def = false) {
        try {
            return n.as<bool>();
        } catch (const YAML::Exception &ex) {
            try {
                return n.as<int>();
            } catch (const YAML::Exception &ex2) {
                qDebug() << ex2.what();
            }
            qDebug() << ex.what();
            return def;
        }
    }

    // NodeChild returns the first defined children or Null Node
    YAML::Node NodeChild(const YAML::Node &n, const std::list<std::string> &keys) {
        for (const auto &key: keys) {
            auto child = n[key];
            if (child.IsDefined()) return child;
        }
        return {};
    }

#endif

    // https://github.com/Dreamacro/clash/wiki/configuration
    void RawUpdater::updateClash(const QString &str) {
#ifndef NKR_NO_YAML
        try {
            auto proxies = YAML::Load(str.toStdString())["proxies"];
            for (auto proxy: proxies) {
                auto type = Node2QString(proxy["type"]).toLower();
                auto type_clash = type;

                if (type == "ss" || type == "ssr") type = "shadowsocks";
                if (type == "socks5") type = "socks";

                auto ent = ProxorGui::ProfileManager::NewProxyEntity(type);
                if (ent->bean->version == -114514) continue;
                bool needFix = false;

                // common
                ent->bean->name = Node2QString(proxy["name"]);
                ent->bean->serverAddress = Node2QString(proxy["server"]);
                ent->bean->serverPort = Node2Int(proxy["port"]);

                if (type_clash == "ss") {
                    auto bean = ent->ShadowSocksBean();
                    bean->method = Node2QString(proxy["cipher"]).replace("dummy", "none");
                    bean->password = Node2QString(proxy["password"]);
                    auto plugin_n = proxy["plugin"];
                    auto pluginOpts_n = proxy["plugin-opts"];

                    // UDP over TCP
                    if (Node2Bool(proxy["udp-over-tcp"])) {
                        bean->uot = Node2Int(proxy["udp-over-tcp-version"]);
                        if (bean->uot == 0) bean->uot = 2;
                    }

                    if (plugin_n.IsDefined() && pluginOpts_n.IsDefined()) {
                        QStringList ssPlugin;
                        auto plugin = Node2QString(plugin_n);
                        if (plugin == "obfs") {
                            ssPlugin << "obfs-local";
                            ssPlugin << "obfs=" + Node2QString(pluginOpts_n["mode"]);
                            ssPlugin << "obfs-host=" + Node2QString(pluginOpts_n["host"]);
                        } else if (plugin == "v2ray-plugin") {
                            auto mode = Node2QString(pluginOpts_n["mode"]);
                            auto host = Node2QString(pluginOpts_n["host"]);
                            auto path = Node2QString(pluginOpts_n["path"]);
                            ssPlugin << "v2ray-plugin";
                            if (!mode.isEmpty() && mode != "websocket") ssPlugin << "mode=" + mode;
                            if (Node2Bool(pluginOpts_n["tls"])) ssPlugin << "tls";
                            if (!host.isEmpty()) ssPlugin << "host=" + host;
                            if (!path.isEmpty()) ssPlugin << "path=" + path;
                            // clash only: skip-cert-verify
                            // clash only: headers
                            // clash: mux=?
                        }
                        bean->SetPluginParts(ssPlugin.isEmpty() ? QString{} : ssPlugin.takeFirst(), ssPlugin.join(";"));
                    }

                    // sing-mux
                    auto smux = NodeChild(proxy, {"smux"});
                    if (Node2Bool(smux["enabled"])) bean->stream->multiplex_status = 1;
                } else if (type == "socks" || type == "http") {
                    auto bean = ent->SocksHTTPBean();
                    bean->username = Node2QString(proxy["username"]);
                    bean->password = Node2QString(proxy["password"]);
                    if (Node2Bool(proxy["tls"])) bean->stream->security = "tls";
                    if (Node2Bool(proxy["skip-cert-verify"])) bean->stream->allow_insecure = true;
                } else if (type == "trojan" || type == "vless") {
                    needFix = true;
                    auto bean = ent->TrojanVLESSBean();
                    if (type == "vless") {
                        bean->flow = Node2QString(proxy["flow"]);
                        bean->password = Node2QString(proxy["uuid"]);
                        // meta packet encoding
                        if (Node2Bool(proxy["packet-addr"])) {
                            bean->stream->packet_encoding = "packetaddr";
                        } else {
                            // For VLESS, default to use xudp
                            bean->stream->packet_encoding = "xudp";
                        }
                    } else {
                        bean->password = Node2QString(proxy["password"]);
                    }
                    bean->stream->security = "tls";
                    bean->stream->network = Node2QString(proxy["network"], "tcp");
                    bean->stream->sni = FIRST_OR_SECOND(Node2QString(proxy["sni"]), Node2QString(proxy["servername"]));
                    bean->stream->alpn = Node2QStringList(proxy["alpn"]).join(",");
                    bean->stream->allow_insecure = Node2Bool(proxy["skip-cert-verify"]);
                    bean->stream->utlsFingerprint = Node2QString(proxy["client-fingerprint"]);
                    if (bean->stream->utlsFingerprint.isEmpty()) {
                        bean->stream->utlsFingerprint = ProxorGui::dataStore->utlsFingerprint;
                    }

                    // sing-mux
                    auto smux = NodeChild(proxy, {"smux"});
                    if (Node2Bool(smux["enabled"])) bean->stream->multiplex_status = 1;

                    // opts
                    auto ws = NodeChild(proxy, {"ws-opts", "ws-opt"});
                    if (ws.IsMap()) {
                        auto headers = ws["headers"];
                        for (auto header: headers) {
                            if (Node2QString(header.first).toLower() == "host") {
                                bean->stream->host = Node2QString(header.second);
                            }
                        }
                        bean->stream->path = Node2QString(ws["path"]);
                        bean->stream->ws_early_data_length = Node2Int(ws["max-early-data"]);
                        bean->stream->ws_early_data_name = Node2QString(ws["early-data-header-name"]);
                    }

                    auto grpc = NodeChild(proxy, {"grpc-opts", "grpc-opt"});
                    if (grpc.IsMap()) {
                        bean->stream->path = Node2QString(grpc["grpc-service-name"]);
                    }

                    auto reality = NodeChild(proxy, {"reality-opts"});
                    if (reality.IsMap()) {
                        bean->stream->reality_pbk = Node2QString(reality["public-key"]);
                        bean->stream->reality_sid = Node2QString(reality["short-id"]);
                    }
                } else if (type == "vmess") {
                    needFix = true;
                    auto bean = ent->VMessBean();
                    bean->uuid = Node2QString(proxy["uuid"]);
                    bean->aid = Node2Int(proxy["alterId"]);
                    bean->security = Node2QString(proxy["cipher"], bean->security);
                    bean->stream->network = Node2QString(proxy["network"], "tcp").replace("h2", "http");
                    bean->stream->sni = FIRST_OR_SECOND(Node2QString(proxy["sni"]), Node2QString(proxy["servername"]));
                    bean->stream->alpn = Node2QStringList(proxy["alpn"]).join(",");
                    if (Node2Bool(proxy["tls"])) bean->stream->security = "tls";
                    if (Node2Bool(proxy["skip-cert-verify"])) bean->stream->allow_insecure = true;
                    bean->stream->utlsFingerprint = Node2QString(proxy["client-fingerprint"]);
                    bean->stream->utlsFingerprint = Node2QString(proxy["client-fingerprint"]);
                    if (bean->stream->utlsFingerprint.isEmpty()) {
                        bean->stream->utlsFingerprint = ProxorGui::dataStore->utlsFingerprint;
                    }

                    // sing-mux
                    auto smux = NodeChild(proxy, {"smux"});
                    if (Node2Bool(smux["enabled"])) bean->stream->multiplex_status = 1;

                    // meta packet encoding
                    if (Node2Bool(proxy["xudp"])) bean->stream->packet_encoding = "xudp";
                    if (Node2Bool(proxy["packet-addr"])) bean->stream->packet_encoding = "packetaddr";

                    // opts
                    auto ws = NodeChild(proxy, {"ws-opts", "ws-opt"});
                    if (ws.IsMap()) {
                        auto headers = ws["headers"];
                        for (auto header: headers) {
                            if (Node2QString(header.first).toLower() == "host") {
                                bean->stream->host = Node2QString(header.second);
                            }
                        }
                        bean->stream->path = Node2QString(ws["path"]);
                        bean->stream->ws_early_data_length = Node2Int(ws["max-early-data"]);
                        bean->stream->ws_early_data_name = Node2QString(ws["early-data-header-name"]);
                        // for Xray
                        if (Node2QString(ws["early-data-header-name"]) == "Sec-WebSocket-Protocol") {
                            bean->stream->path += "?ed=" + Node2QString(ws["max-early-data"]);
                        }
                    }

                    auto grpc = NodeChild(proxy, {"grpc-opts", "grpc-opt"});
                    if (grpc.IsMap()) {
                        bean->stream->path = Node2QString(grpc["grpc-service-name"]);
                    }

                    auto h2 = NodeChild(proxy, {"h2-opts", "h2-opt"});
                    if (h2.IsMap()) {
                        auto hosts = h2["host"];
                        for (auto host: hosts) {
                            bean->stream->host = Node2QString(host);
                            break;
                        }
                        bean->stream->path = Node2QString(h2["path"]);
                    }

                    auto tcp_http = NodeChild(proxy, {"http-opts", "http-opt"});
                    if (tcp_http.IsMap()) {
                        bean->stream->network = "tcp";
                        bean->stream->header_type = "http";
                        auto headers = tcp_http["headers"];
                        for (auto header: headers) {
                            if (Node2QString(header.first).toLower() == "host") {
                                bean->stream->host = Node2QString(header.second[0]);
                            }
                            break;
                        }
                        auto paths = tcp_http["path"];
                        for (auto path: paths) {
                            bean->stream->path = Node2QString(path);
                            break;
                        }
                    }
                } else if (type == "hysteria2") {
                    auto bean = ent->QUICBean();

                    bean->hopPort = Node2QString(proxy["ports"]);

                    bean->allowInsecure = Node2Bool(proxy["skip-cert-verify"]);
                    bean->caText = Node2QString(proxy["ca-str"]);
                    bean->sni = Node2QString(proxy["sni"]);

                    bean->obfsPassword = Node2QString(proxy["obfs-password"]);
                    bean->password = Node2QString(proxy["password"]);

                    bean->uploadMbps = Node2QString(proxy["up"]).split(" ")[0].toInt();
                    bean->downloadMbps = Node2QString(proxy["down"]).split(" ")[0].toInt();
                } else if (type == "tuic") {
                    auto bean = ent->QUICBean();

                    bean->uuid = Node2QString(proxy["uuid"]);
                    bean->password = Node2QString(proxy["password"]);

                    if (Node2Int(proxy["heartbeat-interval"]) != 0) {
                        bean->heartbeat = Int2String(Node2Int(proxy["heartbeat-interval"])) + "ms";
                    }

                    bean->udpRelayMode = Node2QString(proxy["udp-relay-mode"], bean->udpRelayMode);
                    bean->congestionControl = Node2QString(proxy["congestion-controller"], bean->congestionControl);

                    bean->disableSni = Node2Bool(proxy["disable-sni"]);
                    bean->zeroRttHandshake = Node2Bool(proxy["reduce-rtt"]);
                    bean->allowInsecure = Node2Bool(proxy["skip-cert-verify"]);
                    bean->alpn = Node2QStringList(proxy["alpn"]).join(",");
                    bean->caText = Node2QString(proxy["ca-str"]);
                    bean->sni = Node2QString(proxy["sni"]);

                    if (Node2Bool(proxy["udp-over-stream"])) bean->uos = true;

                    if (!Node2QString(proxy["ip"]).isEmpty()) {
                        if (bean->sni.isEmpty()) bean->sni = bean->serverAddress;
                        bean->serverAddress = Node2QString(proxy["ip"]);
                    }
                } else {
                    continue;
                }

                if (needFix) RawUpdater_FixEnt(ent);
                ProxorGui::profileManager->AddProfile(ent, gid_add_to);
                updated_order += ent;
            }
        } catch (const YAML::Exception &ex) {
            runOnUiThread([=] {
                MessageBoxWarning("YAML Exception", ex.what());
            });
        }
#endif
    }

    // 在新的 thread 运行
    void GroupUpdater::AsyncUpdate(const QString &str, int _sub_gid, const std::function<void()> &finish) {
        auto content = str.trimmed();
        bool asURL = false;
        bool createNewGroup = false;

        if (_sub_gid < 0 && (content.startsWith("http://") || content.startsWith("https://"))) {
            auto items = QStringList{
                QObject::tr("As Subscription (new group)"),
                QObject::tr("As link"),
            };
            bool ok;
            auto a = QInputDialog::getItem(nullptr,
                                           QObject::tr("url detected"),
                                           QObject::tr("%1\nHow to update?").arg(content),
                                           items, 0, false, &ok);
            if (!ok) return;
            if (items.indexOf(a) == 0) { asURL = true; createNewGroup = true; }
        }

        runOnNewThread([=] {
            auto gid = _sub_gid;
            if (createNewGroup) {
                auto current = ProxorGui::profileManager->CurrentGroup();
                bool onlyEmptyDefault = (current != nullptr && current->url.isEmpty() &&
                                         current->Profiles().isEmpty() &&
                                         ProxorGui::profileManager->groupsTabOrder.size() == 1);
                auto group = ProxorGui::ProfileManager::NewGroup();
                group->name = QObject::tr("Sub");
                group->url = str;
                ProxorGui::profileManager->AddGroup(group);
                gid = group->id;
                ProxorGui::dataStore->current_group = gid;
                if (onlyEmptyDefault) {
                    // Put the new Sub group first; keep Default as second tab
                    auto &order = ProxorGui::profileManager->groupsTabOrder;
                    order.removeAll(gid);
                    order.prepend(gid);
                    ProxorGui::profileManager->SaveManager();
                }
                MW_dialog_message("SubUpdater", "NewGroup");
            }
            Update(str, gid, asURL);
            emit asyncUpdateCallback(gid);
            if (finish != nullptr) finish();
        });
    }

    void GroupUpdater::Update(const QString &_str, int _sub_gid, bool _not_sub_as_url) {
        // 创建 rawUpdater
        ProxorGui::dataStore->imported_count = 0;
        auto rawUpdater = std::make_unique<RawUpdater>();
        rawUpdater->gid_add_to = _sub_gid;

        // 准备
        QString sub_user_info;
        bool asURL = _sub_gid >= 0 || _not_sub_as_url; // 把 _str 当作 url 处理（下载内容）
        auto content = _str.trimmed();
        const auto subscriptionUrl = QUrl(content);
        auto group = ProxorGui::profileManager->GetGroup(_sub_gid);
        if (group != nullptr && group->archive) return;

        // 网络请求
        if (asURL) {
            auto groupName = group == nullptr ? content : group->name;
            MW_show_log(">>>>>>>> " + QObject::tr("Requesting subscription: %1").arg(groupName));

            auto resp = NetworkRequestHelper::HttpGet(content);
            if (!resp.error.isEmpty()) {
                MW_show_log("<<<<<<<< " + QObject::tr("Requesting subscription %1 error: %2").arg(groupName, resp.error + "\n" + resp.data));
                return;
            }

            content = resp.data;
            sub_user_info = NetworkRequestHelper::GetHeader(resp.header, "Subscription-UserInfo");

            // Parse server-recommended update interval
            auto profileUpdateInterval = NetworkRequestHelper::GetHeader(resp.header, "profile-update-interval");
            if (!profileUpdateInterval.isEmpty()) {
                bool ok = false;
                int intervalMinutes = profileUpdateInterval.trimmed().toInt(&ok);
                if (ok && intervalMinutes > 0) {
                    MW_show_log(QObject::tr("Server recommends update interval: %1 minutes").arg(intervalMinutes));
                }
            }
            auto updateAlways = NetworkRequestHelper::GetHeader(resp.header, "update-always");
            if (updateAlways.toLower() == "true") {
                MW_show_log(QObject::tr("Server indicates always-update mode"));
            }

            auto sub_name = ResolveSubscriptionName(subscriptionUrl, resp.header);
            if (group != nullptr && !sub_name.isEmpty()) {
                group->name = sub_name;
            }

            MW_show_log("<<<<<<<< " + QObject::tr("Subscription request fininshed: %1").arg(groupName));
        }

        QList<std::shared_ptr<ProxorGui::ProxyEntity>> in;          // 更新前
        QList<std::shared_ptr<ProxorGui::ProxyEntity>> out_all;     // 更新前 + 更新后
        QList<std::shared_ptr<ProxorGui::ProxyEntity>> out;         // 更新后
        QList<std::shared_ptr<ProxorGui::ProxyEntity>> only_in;     // 只在更新前有的
        QList<std::shared_ptr<ProxorGui::ProxyEntity>> only_out;    // 只在更新后有的
        QList<std::shared_ptr<ProxorGui::ProxyEntity>> update_del;  // 更新前后都有的，需要删除的新配置
        QList<std::shared_ptr<ProxorGui::ProxyEntity>> update_keep; // 更新前后都有的，被保留的旧配置

        // 订阅解析前
        if (group != nullptr) {
            in = group->Profiles();
            group->sub_last_update = QDateTime::currentMSecsSinceEpoch() / 1000;
            group->info = sub_user_info;
            group->order.clear();
            ProxorGui::profileManager->SaveGroup(group);
            //
            if (ProxorGui::dataStore->sub_clear) {
                MW_show_log(QObject::tr("Clearing servers..."));
                for (const auto &profile: in) {
                    ProxorGui::profileManager->DeleteProfile(profile->id);
                }
            }
        }

        // 解析并添加 profile
        rawUpdater->update(content);

        if (group != nullptr) {
            out_all = group->Profiles();

            QString change_text;

            if (ProxorGui::dataStore->sub_clear) {
                // all is new profile
                for (const auto &ent: out_all) {
                    change_text += "[+] " + ent->bean->DisplayTypeAndName() + "\n";
                }
            } else {
                // find and delete not updated profile by ProfileFilter
                ProxorGui::ProfileFilter::OnlyInSrc_ByPointer(out_all, in, out);
                ProxorGui::ProfileFilter::OnlyInSrc(in, out, only_in);
                ProxorGui::ProfileFilter::OnlyInSrc(out, in, only_out);
                ProxorGui::ProfileFilter::Common(in, out, update_keep, update_del, false);

                QString notice_added;
                QString notice_deleted;
                for (const auto &ent: only_out) {
                    notice_added += "[+] " + ent->bean->DisplayTypeAndName() + "\n";
                }
                for (const auto &ent: only_in) {
                    notice_deleted += "[-] " + ent->bean->DisplayTypeAndName() + "\n";
                }

                // sort according to order in remote
                group->order = {};
                for (const auto &ent: rawUpdater->updated_order) {
                    auto deleted_index = update_del.indexOf(ent);
                    if (deleted_index >= 0) {
                        if (deleted_index >= update_keep.count()) continue; // should not happen
                        auto ent2 = update_keep[deleted_index];
                        group->order.append(ent2->id);
                    } else {
                        group->order.append(ent->id);
                    }
                }
                ProxorGui::profileManager->SaveGroup(group);

                // cleanup
                for (const auto &ent: out_all) {
                    if (!group->order.contains(ent->id)) {
                        ProxorGui::profileManager->DeleteProfile(ent->id);
                    }
                }

                change_text = "\n" + QObject::tr("Added %1 profiles:\n%2\nDeleted %3 Profiles:\n%4")
                                         .arg(only_out.length())
                                         .arg(notice_added)
                                         .arg(only_in.length())
                                         .arg(notice_deleted);
                if (only_out.length() + only_in.length() == 0) change_text = QObject::tr("Nothing");
            }

            auto change_prefix = "<<<<<<<< " + QObject::tr("Change of %1:").arg(group->name);
            auto change_separator = change_text.contains('\n') ? "\n" : " ";
            MW_show_log(change_prefix + change_separator + change_text);
            MW_dialog_message("SubUpdater", "finish-dingyue");
        } else {
            ProxorGui::dataStore->imported_count = rawUpdater->updated_order.count();
            MW_dialog_message("SubUpdater", "finish");
        }
    }
} // namespace ProxorGui_sub

bool UI_update_all_groups_Updating = false;

#define should_skip_group(g) (g == nullptr || g->url.isEmpty() || g->archive || (onlyAllowed && g->skip_auto_update))

void serialUpdateSubscription(const QList<int> &groupsTabOrder, int _order, bool onlyAllowed) {
    if (_order >= groupsTabOrder.size()) {
        UI_update_all_groups_Updating = false;
        return;
    }

    // calculate this group
    auto group = ProxorGui::profileManager->GetGroup(groupsTabOrder[_order]);
    if (group == nullptr || should_skip_group(group)) {
        serialUpdateSubscription(groupsTabOrder, _order + 1, onlyAllowed);
        return;
    }

    int nextOrder = _order + 1;
    while (nextOrder < groupsTabOrder.size()) {
        auto nextGid = groupsTabOrder[nextOrder];
        auto nextGroup = ProxorGui::profileManager->GetGroup(nextGid);
        if (!should_skip_group(nextGroup)) {
            break;
        }
        nextOrder += 1;
    }

    // Async update current group
    UI_update_all_groups_Updating = true;
    ProxorGui_sub::groupUpdater->AsyncUpdate(group->url, group->id, [=] {
        serialUpdateSubscription(groupsTabOrder, nextOrder, onlyAllowed);
    });
}

void UI_update_all_groups(bool onlyAllowed) {
    if (UI_update_all_groups_Updating) {
        MW_show_log("The last subscription update has not exited.");
        return;
    }

    auto groupsTabOrder = ProxorGui::profileManager->groupsTabOrder;
    serialUpdateSubscription(groupsTabOrder, 0, onlyAllowed);
}

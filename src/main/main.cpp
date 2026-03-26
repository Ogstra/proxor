#include <csignal>

#include <QApplication>
#include <QDir>
#include <QMessageBox>
#include <QStandardPaths>
#include <QLocalSocket>
#include <QLocalServer>
#include <QThread>

#include "3rdparty/RunGuard.hpp"
#include "main/ProxorGui.hpp"

#include "ui/mainwindow_interface.h"

#ifdef Q_OS_WIN
#include "sys/windows/MiniDump.h"
#endif

void signal_handler(int signum) {
    if (qApp) {
        GetMainWindow()->on_commitDataRequest();
        qApp->exit();
    }
}


#define LOCAL_SERVER_PREFIX "proxorlocalserver-"

int main(int argc, char* argv[]) {
    // Core dump
#ifdef Q_OS_WIN
    Windows_SetCrashHandler();
#endif

    // pre-init QApplication
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0) && QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
    QApplication::setAttribute(Qt::AA_DisableWindowContextHelpButton);
#endif
#if QT_VERSION >= QT_VERSION_CHECK(5, 7, 0)
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
#endif
    QApplication::setQuitOnLastWindowClosed(false);
    auto preQApp = new QApplication(argc, argv);

    // Clean
    const auto packageRoot = ProxorGui::PackageRootPath();
    QDir::setCurrent(packageRoot);
    if (QFile::exists("updater.old")) {
        QFile::remove("updater.old");
    }
#ifndef Q_OS_WIN
    if (!QFile::exists("updater")) {
        QFile::link("launcher", "updater");
    }
#endif

    // Flags
    ProxorGui::dataStore->argv = QApplication::arguments();
    if (ProxorGui::dataStore->argv.contains("-many")) ProxorGui::dataStore->flag_many = true;
    if (ProxorGui::dataStore->argv.contains("-appdata")) {
        ProxorGui::dataStore->flag_use_appdata = true;
        int appdataIndex = ProxorGui::dataStore->argv.indexOf("-appdata");
        if (ProxorGui::dataStore->argv.size() > appdataIndex + 1 && !ProxorGui::dataStore->argv.at(appdataIndex + 1).startsWith("-")) {
            ProxorGui::dataStore->appdataDir = ProxorGui::dataStore->argv.at(appdataIndex + 1);
        }
    }
    if (ProxorGui::dataStore->argv.contains("-tray")) ProxorGui::dataStore->flag_tray = true;
    if (ProxorGui::dataStore->argv.contains("-debug")) ProxorGui::dataStore->flag_debug = true;
    if (ProxorGui::dataStore->argv.contains("-flag_restart_tun_on")) ProxorGui::dataStore->flag_restart_tun_on = true;
    if (ProxorGui::dataStore->argv.contains("-flag_reorder")) ProxorGui::dataStore->flag_reorder = true;
#ifdef NKR_CPP_USE_APPDATA
    ProxorGui::dataStore->flag_use_appdata = true; // Example: Package & MacOS
#endif
#ifdef NKR_CPP_DEBUG
    ProxorGui::dataStore->flag_debug = true;
#endif

    // dirs & clean
    auto wd = QDir(packageRoot);
    if (ProxorGui::dataStore->flag_use_appdata) {
        QApplication::setApplicationName("proxor");
        if (!ProxorGui::dataStore->appdataDir.isEmpty()) {
            wd.setPath(ProxorGui::dataStore->appdataDir);
        } else {
            wd.setPath(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
        }
    }
    if (!wd.exists()) wd.mkpath(wd.absolutePath());
    if (!wd.exists("config")) wd.mkdir("config");
    QDir::setCurrent(wd.absoluteFilePath("config"));
    QDir("temp").removeRecursively();

    // init QApplication
    delete preQApp;
    QApplication a(argc, argv);

    // dispatchers
    DS_cores = new QThread;
    DS_cores->start();

    // RunGuard
    RunGuard guard("proxor" + wd.absolutePath());
    quint64 guard_data_in = GetRandomUint64();
    quint64 guard_data_out = 0;
    if (!ProxorGui::dataStore->flag_many && !guard.tryToRun(&guard_data_in)) {
        // Some Good System
        if (guard.isAnotherRunning(&guard_data_out)) {
            // Wake up a running instance
            QLocalSocket socket;
            socket.connectToServer(LOCAL_SERVER_PREFIX + Int2String(guard_data_out));
            qDebug() << socket.fullServerName();
            if (!socket.waitForConnected(500)) {
                qDebug() << "Failed to wake a running instance.";
                return 0;
            }
            qDebug() << "connected to local server, try to raise another program";
            socket.write("1");
            socket.waitForBytesWritten(500);
            return 0;
        }
        // Some Bad System
        QMessageBox::warning(nullptr, "Proxor", "RunGuard disallow to run, use -many to force start.");
        return 0;
    }
    MF_release_runguard = [&] { guard.release(); };

// icons
#if (QT_VERSION >= QT_VERSION_CHECK(5, 11, 0))
    QIcon::setFallbackSearchPaths(QStringList{
        ":/proxor",
        ":/icon",
    });
#endif

    // icon for no theme
    if (QIcon::themeName().isEmpty()) {
        QIcon::setThemeName("breeze");
    }

    // Dir
    QDir dir;
    bool dir_success = true;
    if (!dir.exists("profiles")) {
        dir_success &= dir.mkdir("profiles");
    }
    if (!dir.exists("groups")) {
        dir_success &= dir.mkdir("groups");
    }
    if (!dir.exists(ROUTES_PREFIX_NAME)) {
        dir_success &= dir.mkdir(ROUTES_PREFIX_NAME);
    }
    if (!dir_success) {
        QMessageBox::warning(nullptr, "Error", "No permission to write " + dir.absolutePath());
        return 1;
    }

    // Load dataStore
    switch (ProxorGui::coreType) {
        case ProxorGui::CoreType::SING_BOX:
            ProxorGui::dataStore->fn = "groups/proxor.json";
            break;
        default:
            MessageBoxWarning("Error", "Unknown coreType.");
            return 0;
    }
    auto isLoaded = ProxorGui::dataStore->Load();
    if (!isLoaded) {
        ProxorGui::dataStore->Save();
    } else if (!ProxorGui::dataStore->fake_dns_migrated) {
        // One-time migration: enable fake-dns for existing installs upgrading from pre-1.2.3.
        // Without fake-dns, every new connection incurs a 400ms-1.3s DNS round-trip through the proxy.
        ProxorGui::dataStore->fake_dns = true;
        ProxorGui::dataStore->fake_dns_migrated = true;
        ProxorGui::dataStore->Save();
    }

    // Datastore & Flags
    if (ProxorGui::dataStore->start_minimal) ProxorGui::dataStore->flag_tray = true;

    // load routing
    ProxorGui::dataStore->routing = std::make_unique<ProxorGui::Routing>();
    ProxorGui::dataStore->routing->fn = ROUTES_PREFIX + ProxorGui::dataStore->active_routing;
    isLoaded = ProxorGui::dataStore->routing->Load();
    if (!isLoaded) {
        ProxorGui::dataStore->routing->Save();
    } else if (!ProxorGui::dataStore->routing->use_dns_object &&
               ProxorGui::dataStore->routing->direct_dns == "local") {
        // Keep the original direct DoH default outside Tun mode. Tun startup
        // now forces a temporary local resolver in the generated config.
        ProxorGui::dataStore->routing->direct_dns = "https://doh.pub/dns-query";
        ProxorGui::dataStore->routing->Save();
    }


    // Signals
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    // QLocalServer
    QLocalServer server;
    auto server_name = LOCAL_SERVER_PREFIX + Int2String(guard_data_in);
    QLocalServer::removeServer(server_name);
    server.listen(server_name);
    QObject::connect(&server, &QLocalServer::newConnection, &a, [&] {
        auto socket = server.nextPendingConnection();
        qDebug() << "nextPendingConnection:" << server_name << socket;
        socket->deleteLater();
        // raise main window
        MW_dialog_message("", "Raise");
    });

    UI_InitMainWindow();
    return QApplication::exec();
}

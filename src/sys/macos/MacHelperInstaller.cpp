#include "MacHelperInstaller.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QTimer>
#include <QWidget>

#include <memory>

#include <unistd.h>

#include "MacHelperClient.h"
#include "MacHelperService.h"
#include "main/ProxorGui.hpp"

namespace {

using DoneFn = std::function<void(MacAdminScriptResult)>;

QString tr(const char *text) {
    return QCoreApplication::translate("MacHelperInstaller", text);
}

// One installer dialog/install at a time, for every caller (UI thread only).
bool g_installInProgress = false;

constexpr int kReadyPollIntervalMs = 500;
constexpr int kReadyPollAttempts = 20; // 10 s

QString explanation(const QString &feature, MacHelperEnableAction action) {
    QString first;
    switch (action) {
    case MacHelperEnableAction::AskUpdate:
        first = tr("%1 needs an updated Proxor network service, which runs in the background with administrator rights.")
                    .arg(feature);
        break;
    case MacHelperEnableAction::AskReinstall:
        first = tr("%1 needs the Proxor network service, but it is not running or does not accept this user yet "
                   "(for example it was installed by another user). Reinstalling it allows this user.")
                    .arg(feature);
        break;
    case MacHelperEnableAction::AskInstall:
    case MacHelperEnableAction::Proceed:
        first = tr("%1 needs the Proxor network service, which runs in the background with administrator rights.")
                    .arg(feature);
        break;
    }
    return first + QLatin1Char(' ') +
           tr("macOS will ask for your administrator password once to install it and may show a "
              "'Background Items Added' notification: keep Proxor allowed in Login Items & Extensions. "
              "Homebrew removes the service when Proxor is upgraded or uninstalled, so you will be asked again "
              "after an upgrade.");
}

QString sha256Hex(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&f)) return {};
    return QString::fromLatin1(hash.result().toHex());
}

// Runs `/bin/sh <script> <args...>` as root through ONE osascript admin prompt, asynchronously.
// `done` gets the classified result once osascript exits.
void runAdminScript(const QString &script, const QStringList &args, DoneFn done, const QString &prompt = QString()) {
    auto *proc = new QProcess(qApp);
    QObject::connect(proc, &QProcess::finished, qApp,
                     [proc, done](int exitCode, QProcess::ExitStatus status) {
                         const QString err = QString::fromUtf8(proc->readAllStandardError());
                         proc->deleteLater();
                         if (status == QProcess::CrashExit) {
                             done({MacAdminScriptOutcome::Failed, tr("the administrator prompt ended unexpectedly")});
                             return;
                         }
                         done(ClassifyMacAdminScriptResult(exitCode, err));
                     });
    QObject::connect(proc, &QProcess::errorOccurred, qApp, [proc, done](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart) return; // every other error is followed by finished()
        proc->deleteLater();
        done({MacAdminScriptOutcome::Failed, tr("could not start the administrator prompt")});
    });
    proc->start(QStringLiteral("/usr/bin/osascript"),
                {QStringLiteral("-e"), MacAdminAppleScript(QStringLiteral("/bin/sh"), QStringList{script} + args, prompt)});
}

// After a successful install: wait (bounded, one short async probe per tick) until the helper answers.
// Never starts a new probe while one is in flight.
void waitUntilReady(DoneFn done) {
    auto *timer = new QTimer(qApp);
    auto attempts = std::make_shared<int>(0);
    auto inFlight = std::make_shared<bool>(false);
    auto finished = std::make_shared<bool>(false);
    QObject::connect(timer, &QTimer::timeout, timer, [timer, attempts, inFlight, finished, done] {
        if (*finished) return;
        ++*attempts;
        if (!*inFlight) {
            *inFlight = true;
            MacHelperSvc()->probe(timer, 500, [timer, inFlight, finished, done](const MacHelperProbe &, MacHelperState st) {
                *inFlight = false;
                if (*finished || st != MacHelperState::Ready) return;
                *finished = true;
                timer->stop();
                timer->deleteLater();
                done({MacAdminScriptOutcome::Ok, QString()});
            });
        }
        if (*attempts >= kReadyPollAttempts && !*finished) {
            *finished = true;
            timer->stop();
            timer->deleteLater();
            done({MacAdminScriptOutcome::Failed,
                  tr("the service was installed but does not answer; check Login Items & Extensions")});
        }
    });
    timer->start(kReadyPollIntervalMs);
}

void startInstall(DoneFn done) {
    const QString script = MacHelperInstaller::BundledScriptPath(QStringLiteral("helper-install.sh"));
    const QString src = QFileInfo(ProxorGui::PackageExecutablePath(QStringLiteral("proxor_core"))).canonicalFilePath();
    const QString app =
        QDir(QCoreApplication::applicationDirPath() + QStringLiteral("/../..")).canonicalPath();
    if (script.isEmpty() || src.isEmpty() || app.isEmpty()) {
        done({MacAdminScriptOutcome::Failed, tr("installer script not found in the app bundle")});
        return;
    }
    const QString sha = sha256Hex(src);
    if (sha.isEmpty()) {
        done({MacAdminScriptOutcome::Failed, tr("could not read the Proxor core to install")});
        return;
    }
    const QString uid = QString::number(::getuid());
    runAdminScript(script, {uid, src, sha, app}, [done](MacAdminScriptResult result) {
        if (result.outcome != MacAdminScriptOutcome::Ok) {
            done(result);
            return;
        }
        waitUntilReady(done);
    }, tr("Proxor needs administrator rights to install its network service, which turns Tun and System Proxy on and off."));
}

} // namespace

QString MacHelperInstaller::BundledScriptPath(const QString &name) {
    const QDir dir(QCoreApplication::applicationDirPath() + QStringLiteral("/../Resources/helper"));
    return QFileInfo(dir.filePath(name)).canonicalFilePath();
}

void MacHelperInstaller::ConfirmAndInstall(QWidget *parent, const QString &feature, MacHelperEnableAction action,
                                           std::function<void(MacAdminScriptResult)> done) {
    if (action == MacHelperEnableAction::Proceed) {
        done({MacAdminScriptOutcome::Ok, QString()});
        return;
    }
    // Non-modal-loop dialog: the caller regains control immediately and `done` fires later.
    // The in-progress flag is cleared right before every `done` call, so it is false when `done` runs.
    g_installInProgress = true;
    DoneFn finish = [done](MacAdminScriptResult result) {
        g_installInProgress = false;
        done(std::move(result));
    };
    if (action == MacHelperEnableAction::AskInstall) {
        // First install: go straight to the native macOS password prompt (its text explains why).
        startInstall(finish);
        return;
    }
    auto *box = new QMessageBox(QMessageBox::Question, tr("Proxor network service"), explanation(feature, action),
                                QMessageBox::Yes | QMessageBox::No, parent);
    box->setDefaultButton(QMessageBox::Yes);
    box->setAttribute(Qt::WA_DeleteOnClose);
    QObject::connect(box, &QDialog::finished, box, [finish](int result) {
        if (result != QMessageBox::Yes) {
            finish({MacAdminScriptOutcome::Cancelled, QStringLiteral("declined")});
            return;
        }
        startInstall(finish);
    });
    box->open();
}

bool MacHelperInstaller::InstallInProgress() {
    return g_installInProgress;
}

void MacHelperInstaller::Uninstall(QWidget *parent, std::function<void(MacAdminScriptResult)> done) {
    Q_UNUSED(parent);
    // Fallback: the bundled script through one administrator prompt (helper not reachable, or it refused).
    auto viaScript = [done] {
        const QString script = BundledScriptPath(QStringLiteral("helper-uninstall.sh"));
        if (script.isEmpty()) {
            done({MacAdminScriptOutcome::Failed, tr("uninstaller script not found in the app bundle")});
            return;
        }
        runAdminScript(script, {}, [done](MacAdminScriptResult result) {
            if (result.outcome == MacAdminScriptOutcome::Ok) MacHelperSvc()->disconnectFromHelper();
            done(result);
        });
    };
    MacHelperSvc()->probe(qApp, 1000, [done, viaScript](const MacHelperProbe &, MacHelperState st) {
        if (st != MacHelperState::Ready) {
            viaScript();
            return;
        }
        MacHelperSvc()->uninstall(qApp, 10000, [done, viaScript](const MacHelperService::Reply &reply) {
            if (reply.ok) {
                MacHelperSvc()->disconnectFromHelper();
                done({MacAdminScriptOutcome::Ok, QString()});
                return;
            }
            viaScript();
        });
    });
}

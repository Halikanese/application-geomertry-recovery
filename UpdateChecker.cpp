#include "UpdateChecker.h"

#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkReply>
#include <QProcess>
#include <QProgressDialog>
#include <QStandardPaths>
#include <QVersionNumber>

namespace {

QNetworkRequest gitHubRequest(const QUrl &url, const QByteArray &accept)
{
    QNetworkRequest request(url);
    // GitHub rejects API requests without a user agent
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("%1/%2").arg(QApplication::applicationName(),
                                                  QApplication::applicationVersion()));
    request.setRawHeader("Accept", accept);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    return request;
}

// "v1.2.0" or "1.2.0"
QVersionNumber versionFromTag(QString tag)
{
    if (tag.startsWith(u'v', Qt::CaseInsensitive))
        tag.remove(0, 1);
    return QVersionNumber::fromString(tag);
}

} // namespace

UpdateChecker::UpdateChecker(QWidget *window, QObject *parent)
    : QObject(parent)
    , m_window(window)
{
}

QString UpdateChecker::repository()
{
    return QStringLiteral(AGR_UPDATE_REPOSITORY);
}

void UpdateChecker::check()
{
    if (m_busy)
        return;

    if (repository().isEmpty()) {
        QMessageBox::information(m_window, tr("Check for Updates"),
                                 tr("No update repository is configured for this build."));
        return;
    }

    m_busy = true;
    const QUrl url(QStringLiteral("https://api.github.com/repos/%1/releases/latest").arg(repository()));
    QNetworkReply *reply = m_network.get(gitHubRequest(url, "application/vnd.github+json"));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        m_busy = false;
        handleRelease(reply);
    });
}

void UpdateChecker::handleRelease(QNetworkReply *reply)
{
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status == 404) {
        QMessageBox::information(m_window, tr("Check for Updates"),
                                 tr("No release has been published yet."));
        return;
    }
    if (reply->error() != QNetworkReply::NoError) {
        QMessageBox::warning(m_window, tr("Check for Updates"),
                             tr("The update check failed:\n%1").arg(reply->errorString()));
        return;
    }

    const QJsonObject release = QJsonDocument::fromJson(reply->readAll()).object();
    const QString tag = release.value(QStringLiteral("tag_name")).toString();
    const QVersionNumber latest = versionFromTag(tag);
    const QVersionNumber installed = QVersionNumber::fromString(QApplication::applicationVersion());

    if (latest.isNull() || QVersionNumber::compare(latest, installed) <= 0) {
        QMessageBox::information(m_window, tr("Check for Updates"),
                                 tr("Application Geometry Recovery %1 is up to date.")
                                     .arg(installed.toString()));
        return;
    }

    // The setup program is the .exe asset of the release
    QUrl installerUrl;
    QString installerName;
    for (const QJsonValue &value : release.value(QStringLiteral("assets")).toArray()) {
        const QJsonObject asset = value.toObject();
        const QString name = asset.value(QStringLiteral("name")).toString();
        if (name.endsWith(QLatin1String(".exe"), Qt::CaseInsensitive)) {
            installerUrl = QUrl(asset.value(QStringLiteral("browser_download_url")).toString());
            installerName = name;
            break;
        }
    }
    const QUrl releasePage(release.value(QStringLiteral("html_url")).toString());

    if (!installerUrl.isValid()) {
        const auto answer = QMessageBox::question(
            m_window, tr("Update Available"),
            tr("Version %1 is available (installed: %2), but the release contains no setup "
               "program.\n\nOpen the release page?")
                .arg(latest.toString(), installed.toString()));
        if (answer == QMessageBox::Yes)
            QDesktopServices::openUrl(releasePage);
        return;
    }

    const auto answer = QMessageBox::question(
        m_window, tr("Update Available"),
        tr("Version %1 is available (installed: %2).\n\nDownload and install it now? "
           "Application Geometry Recovery will be closed for the installation.")
            .arg(latest.toString(), installed.toString()));
    if (answer == QMessageBox::Yes)
        download(installerUrl, installerName);
}

void UpdateChecker::download(const QUrl &url, const QString &fileName)
{
    m_busy = true;
    auto *progress = new QProgressDialog(tr("Downloading %1…").arg(fileName), tr("Cancel"), 0, 0, m_window);
    progress->setWindowTitle(tr("Update"));
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    progress->setAttribute(Qt::WA_DeleteOnClose);

    QNetworkReply *reply = m_network.get(gitHubRequest(url, "application/octet-stream"));
    connect(reply, &QNetworkReply::downloadProgress, progress, [progress](qint64 received, qint64 total) {
        if (total > 0) {
            progress->setMaximum(int(total / 1024));
            progress->setValue(int(received / 1024));
        }
    });
    connect(progress, &QProgressDialog::canceled, reply, &QNetworkReply::abort);
    connect(reply, &QNetworkReply::finished, this, [this, reply, progress, fileName] {
        reply->deleteLater();
        progress->close();
        m_busy = false;

        if (reply->error() == QNetworkReply::OperationCanceledError)
            return;
        if (reply->error() != QNetworkReply::NoError) {
            QMessageBox::warning(m_window, tr("Update"),
                                 tr("The download failed:\n%1").arg(reply->errorString()));
            return;
        }

        const QString path = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                                 .filePath(fileName);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(reply->readAll()) < 0) {
            QMessageBox::warning(m_window, tr("Update"),
                                 tr("The setup program could not be saved:\n%1")
                                     .arg(QDir::toNativeSeparators(path)));
            return;
        }
        file.close();
        install(path);
    });
}

void UpdateChecker::install(const QString &installer)
{
    if (!QProcess::startDetached(installer, {}, QFileInfo(installer).absolutePath())) {
        QMessageBox::warning(m_window, tr("Update"),
                             tr("The setup program could not be started:\n%1")
                                 .arg(QDir::toNativeSeparators(installer)));
        return;
    }
    // The installer replaces the files of the running application
    QApplication::quit();
}

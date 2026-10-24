#include "ProfileStore.h"
#include "TrayController.h"

#include <QApplication>
#include <QDir>
#include <QLibraryInfo>
#include <QLocale>
#include <QMessageBox>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("LightningSoftware"));
    QApplication::setApplicationName(QStringLiteral("ApplicationGeometryRecovery"));
    QApplication::setApplicationDisplayName(QStringLiteral("Application Geometry Recovery"));
    QApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
    // Keeps running in the system tray when the main window is closed
    QApplication::setQuitOnLastWindowClosed(false);

    // Texts are English, translated to match the system language. qtbase
    // translates the standard buttons (Yes/No, Cancel).
    QTranslator qtTranslator;
    if (qtTranslator.load(QLocale(), QStringLiteral("qtbase"), QStringLiteral("_"),
                          QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
        QApplication::installTranslator(&qtTranslator);
    QTranslator appTranslator;
    if (appTranslator.load(QLocale(), QStringLiteral("ApplicationGeometryRecovery"),
                           QStringLiteral("_"), QStringLiteral(":/i18n")))
        QApplication::installTranslator(&appTranslator);

    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        QMessageBox::critical(nullptr, QApplication::applicationDisplayName(),
                              QObject::tr("No system tray available."));
        return 1;
    }

    // %APPDATA%\LightningSoftware\ApplicationGeometryRecovery\profiles.xml, so that the same
    // file is used when logged in locally and via RDP
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    ProfileStore store(dataDir + QStringLiteral("/profiles.xml"));

    TrayController tray(&store);
    tray.show();

    return app.exec();
}

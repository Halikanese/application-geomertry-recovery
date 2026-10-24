#include "TrayController.h"

#include "AppIcon.h"
#include "ApplicationStarter.h"
#include "MainWindow.h"
#include "ProfileStore.h"
#include "WindowGeometry.h"
#include "WindowWatcher.h"

#include <QApplication>
#include <QCursor>
#include <QInputDialog>
#include <QMenu>
#include <QSet>
#include <QSysInfo>
#include <QSystemTrayIcon>

TrayController::TrayController(ProfileStore *store, QObject *parent)
    : QObject(parent)
    , m_store(store)
    , m_trayIcon(new QSystemTrayIcon(appIcon(), this))
    , m_menu(new QMenu)
{
    m_menu->addAction(tr("Open"), this, &TrayController::openMainWindow);
    m_menu->addAction(tr("Save…"), this, &TrayController::saveProfile);
    m_loadMenu = m_menu->addMenu(tr("Load"));
    m_menu->addSeparator();
    m_menu->addAction(tr("Exit"), qApp, &QApplication::quit);

    // Filled on opening so that the current profiles always appear
    connect(m_loadMenu, &QMenu::aboutToShow, this, &TrayController::rebuildLoadMenu);
    rebuildLoadMenu();

    m_trayIcon->setToolTip(tr("Application Geometry Recovery"));
    m_trayIcon->setContextMenu(m_menu);

    // Windows of applications started later get the geometry of the active profile
    auto *watcher = new WindowWatcher(this);
    connect(watcher, &WindowWatcher::windowOpened, this, &TrayController::placeNewWindow);

    // A left click shows the menu as well
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger)
            m_menu->popup(QCursor::pos());
        else if (reason == QSystemTrayIcon::DoubleClick)
            openMainWindow();
    });
}

TrayController::~TrayController()
{
    delete m_mainWindow;
    delete m_menu;
}

void TrayController::show()
{
    m_trayIcon->show();
}

void TrayController::openMainWindow()
{
    if (!m_mainWindow) {
        m_mainWindow = new MainWindow(m_store);
        m_mainWindow->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_mainWindow, &MainWindow::restoreRequested, this, &TrayController::restoreProfile);
    }
    m_mainWindow->showNormal();
    m_mainWindow->raise();
    m_mainWindow->activateWindow();
}

void TrayController::rebuildLoadMenu()
{
    m_loadMenu->clear();
    const QStringList names = m_store->names();
    if (names.isEmpty()) {
        m_loadMenu->addAction(tr("(no profiles)"))->setEnabled(false);
        return;
    }
    // The active profile (loaded last) is checked
    const QString active = m_store->activeProfile();
    for (const QString &name : names) {
        QAction *action = m_loadMenu->addAction(name, this, [this, name] { restoreProfile(name); });
        action->setCheckable(true);
        action->setChecked(name == active);
    }
}

void TrayController::saveProfile()
{
    // Capture before the dialog so it does not disturb the window order
    Profile profile;
    profile.created = QDateTime::currentDateTime();
    profile.remoteSession = WindowGeometry::isRemoteSession();
    profile.computerName = QSysInfo::machineHostName();
    profile.monitors = WindowGeometry::monitors();
    QList<WindowInfo> windows = WindowGeometry::captureWindows();

    QInputDialog dialog;
    dialog.setWindowTitle(tr("Save Profile"));
    dialog.setWindowIcon(appIcon());
    dialog.setWindowFlag(Qt::WindowStaysOnTopHint);
    dialog.setLabelText(tr("%n window(s) found. Profile name:",
                           nullptr, int(windows.size())));
    dialog.setComboBoxItems(m_store->names());
    dialog.setComboBoxEditable(true);
    dialog.setTextValue(profile.remoteSession ? tr("Remote Desktop") : tr("Local"));
    dialog.activateWindow();
    if (dialog.exec() != QDialog::Accepted)
        return;

    profile.name = dialog.textValue().trimmed();
    if (profile.name.isEmpty())
        return;

    const int captured = int(windows.size());
    int kept = 0;
    const bool updated = m_store->contains(profile.name);
    if (updated) {
        // Update an existing profile: applications that are running get the
        // current data. Saved windows of applications that are not running
        // are kept unchanged, as are the settings of the applications.
        QSet<QString> running;
        for (const WindowInfo &window : std::as_const(windows))
            running.insert(window.executablePath.toLower());
        for (const WindowInfo &window : m_store->profile(profile.name).windows()) {
            // Default geometry and autostart move to the captured window with
            // the same application and instance number
            if (running.contains(window.executablePath.toLower())) {
                for (WindowInfo &captured : windows) {
                    if (captured.matchKey() == window.matchKey() && captured.instance == window.instance) {
                        captured.defaultGeometry = window.defaultGeometry;
                        captured.autostart = window.autostart;
                    }
                }
            }
            if (!running.contains(window.executablePath.toLower())) {
                windows.append(window);
                ++kept;
            }
        }
    }

    profile.setWindows(windows);

    m_store->store(profile);
    m_trayIcon->showMessage(tr("Profile saved"),
                            updated ? tr("\"%1\" updated: %2 windows captured, %3 windows of applications "
                                         "not running kept.")
                                          .arg(profile.name)
                                          .arg(captured)
                                          .arg(kept)
                                    : tr("\"%1\" created, %2 windows.")
                                          .arg(profile.name)
                                          .arg(captured),
                            QSystemTrayIcon::Information, 4000);
}

void TrayController::restoreProfile(const QString &name)
{
    if (!m_store->contains(name))
        return;

    // Windows that open later are placed according to this profile
    m_store->setActiveProfile(name);

    const Profile profile = m_store->profile(name);
    const WindowGeometry::RestoreResult result = WindowGeometry::restore(profile.windows());
    // Windows marked for autostart are started if they are not open
    QList<WindowInfo> toStart;
    for (const WindowInfo &window : result.missing) {
        if (window.autostart)
            toStart.append(window);
    }
    const bool startMissing = !toStart.isEmpty();

    QString summary = tr("%1 restored, %2 not found, %3 failed.")
                          .arg(result.restored)
                          .arg(result.notFound)
                          .arg(result.failed);
    if (startMissing)
        summary += u'\n' + tr("Starting %n application(s)…", nullptr, int(toStart.size()));
    showResult(tr("Profile \"%1\" loaded").arg(name), summary,
               startMissing ? result.failures : result.messages,
               result.failed > 0 || (result.notFound > 0 && !startMissing));

    if (!startMissing)
        return;

    // Started applications get their geometry once their windows appear
    auto *starter = new ApplicationStarter(toStart, this);
    connect(starter, &ApplicationStarter::finished, this,
            [this, name](int started, int restored, const QStringList &messages) {
                showResult(tr("Profile \"%1\" loaded").arg(name),
                           tr("%1 applications started, %2 windows placed.").arg(started).arg(restored),
                           messages, !messages.isEmpty());
            });
    starter->start();
}

void TrayController::showResult(const QString &title, QString text, const QStringList &messages,
                                bool warning)
{
    // The balloon only shows a few lines
    const QStringList details = messages.mid(0, 4);
    if (!details.isEmpty())
        text += u'\n' + details.join(u'\n');
    if (messages.size() > details.size())
        text += tr("\n… and %1 more").arg(messages.size() - details.size());

    m_trayIcon->showMessage(title, text,
                            warning ? QSystemTrayIcon::Warning : QSystemTrayIcon::Information, 6000);
}

void TrayController::placeNewWindow(quintptr handle)
{
    const QString name = m_store->activeProfile();
    if (!name.isEmpty())
        WindowGeometry::placeNewWindow(handle, m_store->profile(name).windows());
}

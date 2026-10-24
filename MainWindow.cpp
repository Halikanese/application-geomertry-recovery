#include "MainWindow.h"

#include "AppIcon.h"
#include "ProfileModel.h"
#include "ProfileStore.h"
#include "UpdateChecker.h"
#include "ui_MainWindow.h"

#include <QDir>
#include <QHeaderView>
#include <QListWidget>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QStatusBar>
#include <QTreeView>

namespace {

QString rectText(const QRect &rect)
{
    return QStringLiteral("%1, %2  %3 × %4")
        .arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height());
}

} // namespace
MainWindow::MainWindow(ProfileStore *store, QWidget *parent)
    : QMainWindow(parent)
    , ui(std::make_unique<Ui::MainWindow>())
    , m_store(store)
    , m_model(new ProfileModel(this))
{
    ui->setupUi(this);
    setWindowIcon(appIcon());

    ui->windowTree->setModel(m_model);
    // Window class and path stay in the model, but are not shown
    ui->windowTree->setColumnHidden(ProfileModel::ClassColumn, true);
    ui->windowTree->setColumnHidden(ProfileModel::PathColumn, true);
    // A bit more height per row, so that the check boxes fit
    ui->windowTree->setStyleSheet(QStringLiteral("QTreeView::item { padding: 3px 0px; }"));
    ui->splitter->setStretchFactor(1, 1);
    ui->splitter->setSizes({220, 980});

    statusBar()->showMessage(tr("File: %1").arg(QDir::toNativeSeparators(m_store->fileName())));

    connect(ui->restoreAction, &QAction::triggered, this, [this] {
        if (!currentProfile().isEmpty())
            emit restoreRequested(currentProfile());
    });
    connect(ui->removeAction, &QAction::triggered, this, &MainWindow::removeCurrentProfile);
    auto *updateChecker = new UpdateChecker(this, this);
    connect(ui->checkUpdatesAction, &QAction::triggered, updateChecker, &UpdateChecker::check);
    connect(ui->profileList, &QListWidget::currentTextChanged, this, &MainWindow::showProfile);
    connect(ui->windowTree, &QWidget::customContextMenuRequested, this, &MainWindow::showEntryMenu);

    // Saving reloads the profile into the model, so not while the model is
    // still handling the change
    connect(m_model, &ProfileModel::profileEdited, this,
            [this] { m_store->store(m_model->profile()); }, Qt::QueuedConnection);
    connect(m_store, &ProfileStore::profilesChanged, this, &MainWindow::reloadProfiles);

    reloadProfiles();
}
MainWindow::~MainWindow() = default;

void MainWindow::reloadProfiles()
{
    const QString previous = currentProfile();
    ui->profileList->clear();
    ui->profileList->addItems(m_store->names());

    const QList<QListWidgetItem *> found = ui->profileList->findItems(previous, Qt::MatchExactly);
    if (!found.isEmpty())
        ui->profileList->setCurrentItem(found.first());
    else if (ui->profileList->count() > 0)
        ui->profileList->setCurrentRow(0);
    else
        showProfile(QString());

    // With the same name currentTextChanged is not emitted, but the profile
    // may have been overwritten.
    if (!found.isEmpty())
        showProfile(previous);
}

QString MainWindow::currentProfile() const
{
    const QListWidgetItem *item = ui->profileList->currentItem();
    return item ? item->text() : QString();
}

void MainWindow::showProfile(const QString &name)
{
    const bool valid = !name.isEmpty() && m_store->contains(name);
    ui->restoreAction->setEnabled(valid);
    ui->removeAction->setEnabled(valid);
    if (!valid) {
        ui->profileInfo->setText(tr("No profile selected."));
        m_model->setProfile(Profile());
        return;
    }

    const Profile profile = m_store->profile(name);

    QStringList monitors;
    for (const MonitorInfo &monitor : profile.monitors) {
        monitors.append(tr("%1: %2, %3 dpi%4")
                            .arg(monitor.name, rectText(monitor.geometry))
                            .arg(monitor.dpi)
                            .arg(monitor.primary ? tr(" (primary)") : QString()));
    }
    ui->profileInfo->setText(
        tr("<b>%1</b><br>Saved: %2 on %3 (%4)<br>Monitors: %5<br>Windows: %6")
            .arg(profile.name.toHtmlEscaped(),
                 QLocale().toString(profile.created, QLocale::ShortFormat),
                 profile.computerName.toHtmlEscaped(),
                 profile.remoteSession ? tr("Remote Desktop") : tr("local"),
                 monitors.join(QStringLiteral("; ")).toHtmlEscaped())
            .arg(profile.windowCount()));

    m_model->setProfile(profile);
    ui->windowTree->expandAll();
    for (int column = 0; column < ProfileModel::ColumnCount; ++column)
        ui->windowTree->resizeColumnToContents(column);
}

void MainWindow::removeCurrentProfile()
{
    const QString name = currentProfile();
    if (name.isEmpty())
        return;
    if (QMessageBox::question(this, tr("Delete Profile"),
                              tr("Delete profile \"%1\"?").arg(name)) != QMessageBox::Yes)
        return;
    m_store->remove(name);
}

void MainWindow::showEntryMenu(const QPoint &position)
{
    const QModelIndex index = ui->windowTree->indexAt(position);
    if (!index.isValid())
        return;

    QMenu menu(this);
    menu.addAction(tr("Delete entry"), this,
                   [this, entry = QPersistentModelIndex(index)] { m_model->removeEntry(entry); });
    menu.exec(ui->windowTree->viewport()->mapToGlobal(position));
}

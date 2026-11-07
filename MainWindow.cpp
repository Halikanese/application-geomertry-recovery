#include "MainWindow.h"

#include "AppIcon.h"
#include "LayoutBridge.h"
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
#include <QQmlContext>
#include <QQuickWidget>
#include <QSignalBlocker>
#include <QSplitter>
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
    , m_layout(new LayoutBridge(m_model, this))
{
    ui->setupUi(this);
    setWindowIcon(appIcon());

    ui->windowTree->setModel(m_model);
    // Dropping an instance on another one swaps their numbers
    ui->windowTree->setDragEnabled(true);
    ui->windowTree->setAcceptDrops(true);
    ui->windowTree->setDropIndicatorShown(true);
    ui->windowTree->setDragDropMode(QAbstractItemView::DragDrop);
    ui->windowTree->setDefaultDropAction(Qt::MoveAction);
    // The selection follows the dragged instance, after the drag has ended
    connect(m_model, &ProfileModel::instancesSwapped, this, [this](int application, int, int toRow) {
        ui->windowTree->setCurrentIndex(m_model->instanceIndex(application, toRow));
    }, Qt::QueuedConnection);
    // Window class and path stay in the model, but are not shown
    //ui->windowTree->setColumnHidden(ProfileModel::ClassColumn, true);
    //ui->windowTree->setColumnHidden(ProfileModel::PathColumn, true);
    // A bit more height per row, so that the check boxes fit
    ui->windowTree->setStyleSheet(QStringLiteral("QTreeView::item { padding: 3px 0px; }"));
    setupLayoutView();
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
    connect(m_model, &ProfileModel::profileEdited, this, [this] {
        m_storingEdit = true;
        m_store->store(m_model->profile());
        m_storingEdit = false;
    }, Qt::QueuedConnection);
    connect(m_store, &ProfileStore::profilesChanged, this, &MainWindow::reloadProfiles);

    reloadProfiles();
}
MainWindow::~MainWindow()
{
    // The QML view uses the bridge, which would otherwise be deleted first
    delete m_layoutView;
}

void MainWindow::reloadProfiles()
{
    const QString previous = currentProfile();
    {
        // The profile is shown once below instead of on every list change
        const QSignalBlocker blocker(ui->profileList);
        ui->profileList->clear();
        ui->profileList->addItems(m_store->names());

        const QList<QListWidgetItem *> found = ui->profileList->findItems(previous, Qt::MatchExactly);
        if (!found.isEmpty())
            ui->profileList->setCurrentItem(found.first());
        else if (ui->profileList->count() > 0)
            ui->profileList->setCurrentRow(0);
    }

    // After an edit in this window the model already shows the saved state;
    // reloading it would lose the selection and the expanded rows
    const QString name = currentProfile();
    if (m_storingEdit && name == previous && m_store->contains(name))
        showProfileInfo(m_store->profile(name));
    else
        showProfile(name);
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
    showProfileInfo(profile);

    m_model->setProfile(profile);
    ui->windowTree->expandAll();
    for (int column = 0; column < ProfileModel::ColumnCount; ++column)
        ui->windowTree->resizeColumnToContents(column);
}

void MainWindow::showProfileInfo(const Profile &profile)
{
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
}

void MainWindow::setupLayoutView()
{
    // The layout view goes below the window list, both in a splitter
    auto *view = new QQuickWidget;
    m_layoutView = view;
    view->setResizeMode(QQuickWidget::SizeRootObjectToView);
    view->setMinimumHeight(150);
    view->rootContext()->setContextProperty(QStringLiteral("layoutBridge"), m_layout);
    view->setSource(QUrl(QStringLiteral("qrc:/qt/qml/ApplicationGeometryRecovery/LayoutView.qml")));

    auto *splitter = new QSplitter(Qt::Vertical);
    ui->profileLayout->replaceWidget(ui->windowTree, splitter);
    splitter->addWidget(ui->windowTree);
    splitter->addWidget(view);
    splitter->setChildrenCollapsible(false);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);

    // Selection in both directions: window list and layout view
    connect(ui->windowTree->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex &current) {
                if (current.isValid() && current.parent().isValid())
                    m_layout->setSelection(current.parent().row(), current.row());
                else
                    m_layout->setSelection(-1, -1);
            });
    connect(m_layout, &LayoutBridge::selectionRequested, this, [this](int application, int instance) {
        if (application < 0) {
            ui->windowTree->selectionModel()->clearCurrentIndex();
            ui->windowTree->clearSelection();
            return;
        }
        const QModelIndex index = m_model->instanceIndex(application, instance);
        ui->windowTree->setCurrentIndex(index);
        ui->windowTree->scrollTo(index);
    });
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

#include "ProfileModel.h"

#include <algorithm>

#include <QDir>

namespace {

// internalId of an index: 0 for applications, application row + 1 for
// instances
constexpr quintptr TopLevel = 0;

QString rectText(const QRect &rect)
{
    return QStringLiteral("%1, %2  %3 × %4")
        .arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height());
}

} // namespace

ProfileModel::ProfileModel(QObject *parent)
    : QAbstractItemModel(parent)
{
}

void ProfileModel::setProfile(const Profile &profile)
{
    beginResetModel();
    m_profile = profile;

    // Applications by name, instances by number
    std::stable_sort(m_profile.applications.begin(), m_profile.applications.end(),
                     [](const ApplicationInfo &a, const ApplicationInfo &b) {
                         return a.processName.compare(b.processName, Qt::CaseInsensitive) < 0;
                     });
    for (ApplicationInfo &application : m_profile.applications) {
        std::stable_sort(application.instances.begin(), application.instances.end(),
                         [](const InstanceInfo &a, const InstanceInfo &b) {
                             return a.instance < b.instance;
                         });
    }
    endResetModel();
}

void ProfileModel::removeEntry(const QModelIndex &index)
{
    if (!index.isValid())
        return;

    if (index.internalId() == TopLevel) {
        beginRemoveRows({}, index.row(), index.row());
        m_profile.applications.removeAt(index.row());
        endRemoveRows();
    } else {
        const int applicationRow = int(index.internalId() - 1);
        QList<InstanceInfo> &instances = m_profile.applications[applicationRow].instances;
        if (instances.size() == 1) {
            // Without windows the application has no entry left
            beginRemoveRows({}, applicationRow, applicationRow);
            m_profile.applications.removeAt(applicationRow);
            endRemoveRows();
        } else {
            beginRemoveRows(index.parent(), index.row(), index.row());
            instances.removeAt(index.row());
            endRemoveRows();
        }
    }
    emit profileEdited();
}

QRect ProfileModel::editableRect(const InstanceInfo &instance)
{
    return instance.state == WindowState::Normal ? instance.windowRect : instance.normalRect;
}

void ProfileModel::setInstanceRect(int applicationRow, int instanceRow, const QRect &rect)
{
    if (applicationRow < 0 || applicationRow >= m_profile.applications.size())
        return;
    QList<InstanceInfo> &instances = m_profile.applications[applicationRow].instances;
    if (instanceRow < 0 || instanceRow >= instances.size())
        return;

    // A normal window is placed with its window rectangle; minimized and
    // maximized windows get their restored position from normalRect
    InstanceInfo &instance = instances[instanceRow];
    if (instance.state == WindowState::Normal)
        instance.windowRect = rect;
    instance.normalRect = rect;
    emit dataChanged(instanceIndex(applicationRow, instanceRow, WindowRectColumn),
                     instanceIndex(applicationRow, instanceRow, NormalRectColumn), {Qt::DisplayRole});
}

void ProfileModel::finishGeometryEdit()
{
    emit profileEdited();
}

QModelIndex ProfileModel::instanceIndex(int applicationRow, int instanceRow, int column) const
{
    return index(instanceRow, column, index(applicationRow, 0));
}
QModelIndex ProfileModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return {};
    if (!parent.isValid())
        return createIndex(row, column, TopLevel);
    return createIndex(row, column, quintptr(parent.row()) + 1);
}

QModelIndex ProfileModel::parent(const QModelIndex &child) const
{
    if (!child.isValid() || child.internalId() == TopLevel)
        return {};
    return createIndex(int(child.internalId() - 1), 0, TopLevel);
}

int ProfileModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return int(m_profile.applications.size());
    if (parent.internalId() == TopLevel && parent.column() == 0)
        return int(m_profile.applications.at(parent.row()).instances.size());
    return 0;
}

int ProfileModel::columnCount(const QModelIndex &) const
{
    return ColumnCount;
}

QVariant ProfileModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return {};
    if (index.internalId() == TopLevel)
        return applicationData(m_profile.applications.at(index.row()), index.column(), role);
    const ApplicationInfo &application = m_profile.applications.at(int(index.internalId() - 1));
    return instanceData(application.instances.at(index.row()), index.column(), role);
}

QVariant ProfileModel::applicationData(const ApplicationInfo &application, int column, int role) const
{
    if (role == Qt::DisplayRole) {
        switch (column) {
        case ApplicationColumn:
            return application.processName;
        case InstanceColumn:
            return application.instances.size();
        case PathColumn:
            return QDir::toNativeSeparators(application.executablePath);
        default:
            break;
        }
    } else if (role == Qt::ToolTipRole && column == PathColumn) {
        return QDir::toNativeSeparators(application.executablePath);
    }
    return {};
}

QVariant ProfileModel::instanceData(const InstanceInfo &instance, int column, int role) const
{
    if (role == Qt::CheckStateRole && column == DefaultColumn)
        return instance.defaultGeometry ? Qt::Checked : Qt::Unchecked;
    if (role == Qt::CheckStateRole && column == AutostartColumn)
        return instance.autostart ? Qt::Checked : Qt::Unchecked;
    if (role == Qt::CheckStateRole && column == InstanceColumn)
        return instance.matchInstance ? Qt::Checked : Qt::Unchecked;
    if (role == Qt::CheckStateRole && column == ClassColumn)
        return instance.matchClass ? Qt::Checked : Qt::Unchecked;
    if (role == Qt::CheckStateRole && column == TitleColumn)
        return instance.matchTitle ? Qt::Checked : Qt::Unchecked;
    if (role == Qt::ToolTipRole && column == TitleColumn)
        return instance.title;
    if (role != Qt::DisplayRole)
        return {};

    switch (column) {
    case InstanceColumn:
        return instance.instance + 1;
    case StateColumn:
        return windowStateText(instance.state);
    case WindowRectColumn:
        return rectText(instance.windowRect);
    case NormalRectColumn:
        return rectText(instance.normalRect);
    case MonitorColumn:
        return instance.monitorName;
    case ClassColumn:
        return instance.className;
    case TitleColumn:
        return instance.title;
    default:
        break;
    }
    return {};
}

bool ProfileModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || role != Qt::CheckStateRole)
        return false;

    if (index.internalId() == TopLevel)
        return false;

    // Check boxes that only concern this instance
    InstanceInfo &instance = m_profile.applications[int(index.internalId() - 1)].instances[index.row()];
    bool *flag = nullptr;
    switch (index.column()) {
    case AutostartColumn:
        flag = &instance.autostart;
        break;
    case InstanceColumn:
        flag = &instance.matchInstance;
        break;
    case ClassColumn:
        flag = &instance.matchClass;
        break;
    case TitleColumn:
        flag = &instance.matchTitle;
        break;
    default:
        break;
    }
    if (flag) {
        *flag = value.value<Qt::CheckState>() == Qt::Checked;
        emit dataChanged(index, index, {Qt::CheckStateRole});
        emit profileEdited();
        return true;
    }
    if (index.column() != DefaultColumn)
        return false;

    const int applicationRow = int(index.internalId() - 1);
    const bool checked = value.value<Qt::CheckState>() == Qt::Checked;
    const QString path = m_profile.applications.at(applicationRow).executablePath;

    // At most one default per executable path, also across window classes
    for (int a = 0; a < m_profile.applications.size(); ++a) {
        ApplicationInfo &application = m_profile.applications[a];
        if (application.executablePath.compare(path, Qt::CaseInsensitive) != 0)
            continue;
        for (int i = 0; i < application.instances.size(); ++i) {
            const bool wanted = checked && a == applicationRow && i == index.row();
            if (application.instances.at(i).defaultGeometry == wanted)
                continue;
            application.instances[i].defaultGeometry = wanted;
            const QModelIndex changed = createIndex(i, DefaultColumn, quintptr(a) + 1);
            emit dataChanged(changed, changed, {Qt::CheckStateRole});
        }
    }
    emit profileEdited();
    return true;
}

Qt::ItemFlags ProfileModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    Qt::ItemFlags result = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (index.internalId() != TopLevel) {
        switch (index.column()) {
        case InstanceColumn:
        case DefaultColumn:
        case AutostartColumn:
        case ClassColumn:
        case TitleColumn:
            result |= Qt::ItemIsUserCheckable;
            break;
        default:
            break;
        }
    }
    return result;
}

QVariant ProfileModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal)
        return {};

    if (role == Qt::ToolTipRole && section == DefaultColumn)
        return tr("Used for further windows of the application when all its saved entries are "
                  "in use. At most one per application.");
    if (role == Qt::ToolTipRole && section == InstanceColumn)
        return tr("Checked: only an open window with the same instance number is used for this entry.");
    if (role == Qt::ToolTipRole && section == ClassColumn)
        return tr("Checked: only an open window with the same window class is used for this entry.");
    if (role == Qt::ToolTipRole && section == TitleColumn)
        return tr("Checked: only an open window with the same title is used for this entry.");
    if (role == Qt::ToolTipRole && section == AutostartColumn)
        return tr("Start the application for this window when the profile is loaded and the window is not open.");
    if (role != Qt::DisplayRole)
        return {};

    switch (section) {
    case ApplicationColumn:
        return tr("Application");
    case InstanceColumn:
        return tr("Instance");
    case DefaultColumn:
        return tr("Default geometry");
    case AutostartColumn:
        return tr("Autostart");
    case StateColumn:
        return tr("State");
    case WindowRectColumn:
        return tr("Window (x, y  W × H)");
    case NormalRectColumn:
        return tr("Normal position");
    case MonitorColumn:
        return tr("Monitor");
    case ClassColumn:
        return tr("Window class");
    case TitleColumn:
        return tr("Title");
    case PathColumn:
        return tr("Path");
    default:
        break;
    }
    return {};
}

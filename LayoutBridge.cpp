#include "LayoutBridge.h"

#include "ProfileModel.h"

#include <QColor>

namespace {

// Distinct colors for the windows: hues spread by the golden angle
QColor windowColor(int index)
{
    return QColor::fromHsv((index * 137) % 360, 170, 220);
}

} // namespace

LayoutBridge::LayoutBridge(ProfileModel *model, QObject *parent)
    : QObject(parent)
    , m_model(model)
{
    // Geometry changes from the view itself must not rebuild the view, so
    // only structural changes of the model are followed
    connect(m_model, &QAbstractItemModel::modelReset, this, [this] {
        setSelection(-1, -1);
        emit layoutChanged();
    });
    connect(m_model, &QAbstractItemModel::rowsRemoved, this, [this] {
        setSelection(-1, -1);
        emit layoutChanged();
    });
}

QVariantList LayoutBridge::monitors() const
{
    QVariantList result;
    for (const MonitorInfo &monitor : m_model->profile().monitors) {
        QString name = monitor.name;
        name.remove(QStringLiteral("\\\\.\\"));
        // Without a saved work area the whole monitor is usable
        const QRect work = monitor.workArea.isValid() ? monitor.workArea : monitor.geometry;
        result.append(QVariantMap{
            {QStringLiteral("name"), name},
            {QStringLiteral("x"), monitor.geometry.x()},
            {QStringLiteral("y"), monitor.geometry.y()},
            {QStringLiteral("width"), monitor.geometry.width()},
            {QStringLiteral("height"), monitor.geometry.height()},
            {QStringLiteral("primary"), monitor.primary},
            {QStringLiteral("workX"), work.x()},
            {QStringLiteral("workY"), work.y()},
            {QStringLiteral("workWidth"), work.width()},
            {QStringLiteral("workHeight"), work.height()},
        });
    }
    return result;
}

QVariantList LayoutBridge::windows() const
{
    QVariantList result;
    const QList<ApplicationInfo> &applications = m_model->profile().applications;
    int colorIndex = 0;
    for (int a = 0; a < applications.size(); ++a) {
        const ApplicationInfo &application = applications.at(a);
        for (int i = 0; i < application.instances.size(); ++i) {
            const InstanceInfo &instance = application.instances.at(i);
            const QRect rect = ProfileModel::editableRect(instance);
            result.append(QVariantMap{
                {QStringLiteral("application"), a},
                {QStringLiteral("instance"), i},
                {QStringLiteral("name"), application.processName},
                {QStringLiteral("number"), instance.instance + 1},
                {QStringLiteral("x"), rect.x()},
                {QStringLiteral("y"), rect.y()},
                {QStringLiteral("width"), rect.width()},
                {QStringLiteral("height"), rect.height()},
                {QStringLiteral("color"), windowColor(colorIndex++)},
            });
        }
    }
    return result;
}

void LayoutBridge::setSelection(int applicationRow, int instanceRow)
{
    if (applicationRow == m_selectedApplication && instanceRow == m_selectedInstance)
        return;
    m_selectedApplication = applicationRow;
    m_selectedInstance = instanceRow;
    emit selectionChanged();
}

void LayoutBridge::select(int applicationRow, int instanceRow)
{
    setSelection(applicationRow, instanceRow);
    emit selectionRequested(applicationRow, instanceRow);
}

void LayoutBridge::setWindowRect(int applicationRow, int instanceRow, int x, int y, int width, int height)
{
    m_model->setInstanceRect(applicationRow, instanceRow, QRect(x, y, width, height));
}

void LayoutBridge::finishEdit()
{
    m_model->finishGeometryEdit();
}

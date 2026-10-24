#include "ProfileStore.h"

#include "XmlSettingsBackend.h"

#include <algorithm>

#include <QSettings>

// File layout:
//
//   <settings>
//     <group name="profiles">
//       <group name="1">
//         <value name="name" type="QString">Local</value>
//         <group name="monitors"> ... </group>
//         <group name="applications">
//           <group name="1">
//             <value name="processName" type="QString">notepad.exe</value>
//             <group name="instances">
//               <group name="1">
//                 <group name="windowRect">
//                   <value name="x" type="int">100</value> ...

namespace {

void writeRect(QSettings &settings, const QString &group, const QRect &rect)
{
    settings.beginGroup(group);
    settings.setValue(QStringLiteral("x"), rect.x());
    settings.setValue(QStringLiteral("y"), rect.y());
    settings.setValue(QStringLiteral("width"), rect.width());
    settings.setValue(QStringLiteral("height"), rect.height());
    settings.endGroup();
}

QRect readRect(QSettings &settings, const QString &group)
{
    settings.beginGroup(group);
    const QRect rect(settings.value(QStringLiteral("x")).toInt(),
                     settings.value(QStringLiteral("y")).toInt(),
                     settings.value(QStringLiteral("width")).toInt(),
                     settings.value(QStringLiteral("height")).toInt());
    settings.endGroup();
    return rect;
}

void writePoint(QSettings &settings, const QString &group, const QPoint &point)
{
    settings.beginGroup(group);
    settings.setValue(QStringLiteral("x"), point.x());
    settings.setValue(QStringLiteral("y"), point.y());
    settings.endGroup();
}

QPoint readPoint(QSettings &settings, const QString &group)
{
    settings.beginGroup(group);
    const QPoint point(settings.value(QStringLiteral("x")).toInt(),
                       settings.value(QStringLiteral("y")).toInt());
    settings.endGroup();
    return point;
}

QString stateKey(WindowState state)
{
    switch (state) {
    case WindowState::Minimized:
        return QStringLiteral("minimized");
    case WindowState::Maximized:
        return QStringLiteral("maximized");
    case WindowState::Normal:
        break;
    }
    return QStringLiteral("normal");
}

WindowState stateFromKey(const QString &key)
{
    if (key == QLatin1String("minimized"))
        return WindowState::Minimized;
    if (key == QLatin1String("maximized"))
        return WindowState::Maximized;
    return WindowState::Normal;
}

void readIdentity(QSettings &settings, ApplicationIdentity &identity)
{
    identity.executablePath = settings.value(QStringLiteral("executablePath")).toString();
    identity.processName = settings.value(QStringLiteral("processName")).toString();
    identity.className = settings.value(QStringLiteral("className")).toString();
}

void writeIdentity(QSettings &settings, const ApplicationIdentity &identity)
{
    settings.setValue(QStringLiteral("executablePath"), identity.executablePath);
    settings.setValue(QStringLiteral("processName"), identity.processName);
    settings.setValue(QStringLiteral("className"), identity.className);
}

// autostartDefault: for files that store the setting per application or profile
InstanceInfo readInstance(QSettings &settings, bool autostartDefault)
{
    InstanceInfo instance;
    instance.instance = settings.value(QStringLiteral("instance")).toInt();
    instance.state = stateFromKey(settings.value(QStringLiteral("state")).toString());
    instance.windowRect = readRect(settings, QStringLiteral("windowRect"));
    instance.normalRect = readRect(settings, QStringLiteral("normalRect"));
    instance.minPosition = readPoint(settings, QStringLiteral("minPosition"));
    instance.maxPosition = readPoint(settings, QStringLiteral("maxPosition"));
    instance.placementFlags = settings.value(QStringLiteral("placementFlags")).toUInt();
    instance.monitorName = settings.value(QStringLiteral("monitorName")).toString();
    instance.monitorRect = readRect(settings, QStringLiteral("monitorRect"));
    instance.processId = settings.value(QStringLiteral("processId")).toLongLong();
    instance.defaultGeometry = settings.value(QStringLiteral("defaultGeometry")).toBool();
    instance.autostart = settings.value(QStringLiteral("autostart"), autostartDefault).toBool();
    return instance;
}

void writeInstance(QSettings &settings, const InstanceInfo &instance)
{
    settings.setValue(QStringLiteral("instance"), instance.instance);
    settings.setValue(QStringLiteral("state"), stateKey(instance.state));
    writeRect(settings, QStringLiteral("windowRect"), instance.windowRect);
    writeRect(settings, QStringLiteral("normalRect"), instance.normalRect);
    writePoint(settings, QStringLiteral("minPosition"), instance.minPosition);
    writePoint(settings, QStringLiteral("maxPosition"), instance.maxPosition);
    settings.setValue(QStringLiteral("placementFlags"), instance.placementFlags);
    settings.setValue(QStringLiteral("monitorName"), instance.monitorName);
    writeRect(settings, QStringLiteral("monitorRect"), instance.monitorRect);
    settings.setValue(QStringLiteral("processId"), instance.processId);
    if (instance.defaultGeometry)
        settings.setValue(QStringLiteral("defaultGeometry"), true);
    settings.setValue(QStringLiteral("autostart"), instance.autostart);
}

} // namespace

ProfileStore::ProfileStore(const QString &fileName, QObject *parent)
    : QObject(parent)
    , m_fileName(fileName)
{
    load();
}

QStringList ProfileStore::names() const
{
    QStringList result;
    for (const Profile &profile : m_profiles)
        result.append(profile.name);
    result.sort(Qt::CaseInsensitive);
    return result;
}

bool ProfileStore::contains(const QString &name) const
{
    return std::any_of(m_profiles.cbegin(), m_profiles.cend(),
                       [&](const Profile &profile) { return profile.name == name; });
}

Profile ProfileStore::profile(const QString &name) const
{
    for (const Profile &profile : m_profiles) {
        if (profile.name == name)
            return profile;
    }
    return Profile();
}

void ProfileStore::store(const Profile &profile)
{
    auto it = std::find_if(m_profiles.begin(), m_profiles.end(),
                           [&](const Profile &existing) { return existing.name == profile.name; });
    if (it != m_profiles.end())
        *it = profile;
    else
        m_profiles.append(profile);
    save();
    emit profilesChanged();
}

void ProfileStore::remove(const QString &name)
{
    const auto removed = m_profiles.removeIf([&](const Profile &profile) { return profile.name == name; });
    if (removed == 0)
        return;
    save();
    if (m_activeProfile == name)
        setActiveProfile(QString());
    emit profilesChanged();
}

void ProfileStore::setActiveProfile(const QString &name)
{
    if (m_activeProfile == name)
        return;
    m_activeProfile = name;

    QSettings settings(m_fileName, XmlSettingsBackend::format());
    if (name.isEmpty())
        settings.remove(QStringLiteral("activeProfile"));
    else
        settings.setValue(QStringLiteral("activeProfile"), name);
    settings.sync();
    emit activeProfileChanged();
}

void ProfileStore::load()
{
    m_profiles.clear();
    QSettings settings(m_fileName, XmlSettingsBackend::format());
    m_activeProfile = settings.value(QStringLiteral("activeProfile")).toString();

    const int profileCount = settings.beginReadArray(QStringLiteral("profiles"));
    for (int p = 0; p < profileCount; ++p) {
        settings.setArrayIndex(p);
        Profile profile;
        profile.name = settings.value(QStringLiteral("name")).toString();
        profile.created = settings.value(QStringLiteral("created")).toDateTime();
        profile.remoteSession = settings.value(QStringLiteral("remoteSession")).toBool();
        // Older files have one setting for all applications
        const bool startAll = settings.value(QStringLiteral("startApplications")).toBool();
        profile.computerName = settings.value(QStringLiteral("computerName")).toString();

        const int monitorCount = settings.beginReadArray(QStringLiteral("monitors"));
        for (int m = 0; m < monitorCount; ++m) {
            settings.setArrayIndex(m);
            MonitorInfo monitor;
            monitor.name = settings.value(QStringLiteral("name")).toString();
            monitor.geometry = readRect(settings, QStringLiteral("geometry"));
            monitor.workArea = readRect(settings, QStringLiteral("workArea"));
            monitor.dpi = settings.value(QStringLiteral("dpi"), 96).toInt();
            monitor.primary = settings.value(QStringLiteral("primary")).toBool();
            profile.monitors.append(monitor);
        }
        settings.endArray();

        const int applicationCount = settings.beginReadArray(QStringLiteral("applications"));
        for (int a = 0; a < applicationCount; ++a) {
            settings.setArrayIndex(a);
            ApplicationInfo application;
            readIdentity(settings, application);
            const bool startApplication = settings.value(QStringLiteral("autostart"), startAll).toBool();
            const int instanceCount = settings.beginReadArray(QStringLiteral("instances"));
            for (int i = 0; i < instanceCount; ++i) {
                settings.setArrayIndex(i);
                application.instances.append(readInstance(settings, startApplication));
            }
            settings.endArray();
            profile.applications.append(application);
        }
        settings.endArray();

        // Older files store a flat list of windows
        if (applicationCount == 0) {
            QList<WindowInfo> windows;
            const int windowCount = settings.beginReadArray(QStringLiteral("windows"));
            for (int w = 0; w < windowCount; ++w) {
                settings.setArrayIndex(w);
                WindowInfo window;
                readIdentity(settings, window);
                static_cast<InstanceInfo &>(window) = readInstance(settings, startAll);
                windows.append(window);
            }
            settings.endArray();
            profile.setWindows(windows);
        }
        if (!profile.name.isEmpty())
            m_profiles.append(profile);
    }
    settings.endArray();
}

void ProfileStore::save() const
{
    QSettings settings(m_fileName, XmlSettingsBackend::format());
    settings.remove(QStringLiteral("profiles"));

    settings.beginWriteArray(QStringLiteral("profiles"), int(m_profiles.size()));
    for (int p = 0; p < m_profiles.size(); ++p) {
        const Profile &profile = m_profiles.at(p);
        settings.setArrayIndex(p);
        settings.setValue(QStringLiteral("name"), profile.name);
        settings.setValue(QStringLiteral("created"), profile.created);
        settings.setValue(QStringLiteral("remoteSession"), profile.remoteSession);
        settings.setValue(QStringLiteral("computerName"), profile.computerName);

        settings.beginWriteArray(QStringLiteral("monitors"), int(profile.monitors.size()));
        for (int m = 0; m < profile.monitors.size(); ++m) {
            const MonitorInfo &monitor = profile.monitors.at(m);
            settings.setArrayIndex(m);
            settings.setValue(QStringLiteral("name"), monitor.name);
            writeRect(settings, QStringLiteral("geometry"), monitor.geometry);
            writeRect(settings, QStringLiteral("workArea"), monitor.workArea);
            settings.setValue(QStringLiteral("dpi"), monitor.dpi);
            settings.setValue(QStringLiteral("primary"), monitor.primary);
        }
        settings.endArray();

        settings.beginWriteArray(QStringLiteral("applications"), int(profile.applications.size()));
        for (int a = 0; a < profile.applications.size(); ++a) {
            const ApplicationInfo &application = profile.applications.at(a);
            settings.setArrayIndex(a);
            writeIdentity(settings, application);
            settings.beginWriteArray(QStringLiteral("instances"), int(application.instances.size()));
            for (int i = 0; i < application.instances.size(); ++i) {
                settings.setArrayIndex(i);
                writeInstance(settings, application.instances.at(i));
            }
            settings.endArray();
        }
        settings.endArray();
    }    settings.endArray();
    settings.sync();
}

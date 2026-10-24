#include "ApplicationStarter.h"

#include "WindowGeometry.h"

#include <QFileInfo>
#include <QProcess>

namespace {

// Applications get some time to show their window before it is placed
constexpr int PollInterval = 1000;
constexpr int FirstPollDelay = 2000;
constexpr int MaxPolls = 30;

QString describe(const WindowInfo &info)
{
    return QStringLiteral("%1 [%2]").arg(info.processName, QString::number(info.instance + 1));
}

} // namespace

ApplicationStarter::ApplicationStarter(const QList<WindowInfo> &windows, QObject *parent)
    : QObject(parent)
    , m_pending(windows)
{
    m_timer.setInterval(PollInterval);
    connect(&m_timer, &QTimer::timeout, this, &ApplicationStarter::poll);
}

void ApplicationStarter::start()
{
    // Windows that are already open belong to other saved entries
    m_knownHandles = WindowGeometry::windowHandles();

    // One process per missing window, so that each instance gets its own window
    QList<WindowInfo> launched;
    for (const WindowInfo &window : std::as_const(m_pending)) {
        const QFileInfo executable(window.executablePath);
        if (!executable.isFile()) {
            m_messages.append(tr("Not startable: %1").arg(describe(window)));
            continue;
        }
        if (QProcess::startDetached(executable.absoluteFilePath(), {}, executable.absolutePath())) {
            ++m_started;
            launched.append(window);
        } else {
            m_messages.append(tr("Start failed: %1").arg(describe(window)));
        }
    }
    m_pending = launched;

    if (m_pending.isEmpty()) {
        finish();
        return;
    }
    QTimer::singleShot(FirstPollDelay, this, [this] {
        poll();
        if (!m_pending.isEmpty())
            m_timer.start();
    });
}

void ApplicationStarter::poll()
{
    ++m_polls;
    const WindowGeometry::RestoreResult result = WindowGeometry::restore(m_pending, m_knownHandles);
    m_restored += result.restored;
    m_knownHandles.unite(result.usedHandles);
    m_pending = result.missing;

    // Failures of matched windows are final, missing ones are tried again
    m_messages.append(result.failures);

    if (m_pending.isEmpty() || m_polls >= MaxPolls)
        finish();
}

void ApplicationStarter::finish()
{
    m_timer.stop();
    for (const WindowInfo &window : std::as_const(m_pending))
        m_messages.append(tr("No window appeared: %1").arg(describe(window)));
    emit finished(m_started, m_restored, m_messages);
    deleteLater();
}

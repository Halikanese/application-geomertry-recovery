#include "DisplayWatcher.h"

#include "WindowGeometry.h"

#include <QGuiApplication>
#include <QScreen>

namespace {

// A change arrives as several signals, and windows are still moving meanwhile
constexpr int SettleDelay = 2000;

} // namespace

DisplayWatcher::DisplayWatcher(QObject *parent)
    : QObject(parent)
    , m_signature(monitorSignature(WindowGeometry::monitors()))
{
    m_settleTimer.setSingleShot(true);
    m_settleTimer.setInterval(SettleDelay);
    connect(&m_settleTimer, &QTimer::timeout, this, &DisplayWatcher::check);

    auto *application = qGuiApp;
    connect(application, &QGuiApplication::screenAdded, this, [this](QScreen *screen) {
        watchScreen(screen);
        m_settleTimer.start();
    });
    connect(application, &QGuiApplication::screenRemoved, &m_settleTimer, qOverload<>(&QTimer::start));
    connect(application, &QGuiApplication::primaryScreenChanged, &m_settleTimer, qOverload<>(&QTimer::start));
    for (QScreen *screen : QGuiApplication::screens())
        watchScreen(screen);
}

void DisplayWatcher::watchScreen(QScreen *screen)
{
    connect(screen, &QScreen::geometryChanged, &m_settleTimer, qOverload<>(&QTimer::start));
}

void DisplayWatcher::check()
{
    const QStringList signature = monitorSignature(WindowGeometry::monitors());
    // No monitors at all, e.g. during a session change: wait for the next one
    if (signature.isEmpty() || signature == m_signature)
        return;
    m_signature = signature;
    emit configurationChanged();
}

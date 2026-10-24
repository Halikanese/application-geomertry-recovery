#include "WindowWatcher.h"

#include "WindowGeometry.h"

#include <QTimer>

#include <windows.h>

namespace {

// Time the application gets after showing its window
constexpr int SettleDelay = 1000;

void CALLBACK winEventProc(HWINEVENTHOOK, DWORD event, HWND hwnd, LONG idObject, LONG idChild,
                           DWORD, DWORD)
{
    // Only events of the window itself, not of its child objects
    if (hwnd && idObject == OBJID_WINDOW && idChild == CHILDID_SELF)
        WindowWatcher::handleEventForHook(event, reinterpret_cast<quintptr>(hwnd));
}

} // namespace

WindowWatcher *WindowWatcher::s_instance = nullptr;

WindowWatcher::WindowWatcher(QObject *parent)
    : QObject(parent)
{
    Q_ASSERT(!s_instance);
    s_instance = this;

    // Windows that are already open are not new
    m_knownWindows = WindowGeometry::windowHandles();

    // Out of context: the callback runs in this thread's message loop
    const DWORD flags = WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS;
    m_showHook = SetWinEventHook(EVENT_OBJECT_SHOW, EVENT_OBJECT_SHOW, nullptr, winEventProc,
                                 0, 0, flags);
    m_destroyHook = SetWinEventHook(EVENT_OBJECT_DESTROY, EVENT_OBJECT_DESTROY, nullptr,
                                    winEventProc, 0, 0, flags);
}

WindowWatcher::~WindowWatcher()
{
    if (m_showHook)
        UnhookWinEvent(static_cast<HWINEVENTHOOK>(m_showHook));
    if (m_destroyHook)
        UnhookWinEvent(static_cast<HWINEVENTHOOK>(m_destroyHook));
    s_instance = nullptr;
}

void WindowWatcher::handleEventForHook(quint32 event, quintptr handle)
{
    if (s_instance)
        s_instance->handleEvent(event, handle);
}

void WindowWatcher::handleEvent(quint32 event, quintptr handle)
{
    const HWND hwnd = reinterpret_cast<HWND>(handle);

    if (event == EVENT_OBJECT_DESTROY) {
        // Handles are reused by Windows
        m_knownWindows.remove(handle);
        return;
    }

    if (GetAncestor(hwnd, GA_ROOT) != hwnd || m_knownWindows.contains(handle))
        return;
    m_knownWindows.insert(handle);

    QTimer::singleShot(SettleDelay, this, [this, handle] {
        if (IsWindow(reinterpret_cast<HWND>(handle)))
            emit windowOpened(handle);
    });
}

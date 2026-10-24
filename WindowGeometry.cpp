#include "WindowGeometry.h"

#include <algorithm>

#include <QCoreApplication>
#include <QFileInfo>
#include <QHash>

#include <windows.h>
#include <dwmapi.h>
#include <shellscalingapi.h>

namespace {

struct LiveWindow
{
    HWND hwnd = nullptr;
    WindowInfo info;
};

QRect toQRect(const RECT &rect)
{
    return QRect(QPoint(rect.left, rect.top), QPoint(rect.right - 1, rect.bottom - 1));
}

RECT toRect(const QRect &rect)
{
    return RECT{rect.left(), rect.top(), rect.left() + rect.width(), rect.top() + rect.height()};
}

QString windowClass(HWND hwnd)
{
    wchar_t buffer[256] = {};
    const int length = GetClassNameW(hwnd, buffer, 256);
    return QString::fromWCharArray(buffer, length);
}

QString processPath(DWORD processId)
{
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (!process)
        return QString();
    wchar_t buffer[MAX_PATH * 2] = {};
    DWORD size = MAX_PATH * 2;
    QString path;
    if (QueryFullProcessImageNameW(process, 0, buffer, &size))
        path = QString::fromWCharArray(buffer, int(size));
    CloseHandle(process);
    return path;
}

bool isCloaked(HWND hwnd)
{
    DWORD cloaked = 0;
    return SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked)))
           && cloaked != 0;
}

// Same rules as the taskbar: visible, not cloaked (DWM), no tool window
// and no owned window, unless it forces WS_EX_APPWINDOW.
bool isApplicationWindow(HWND hwnd)
{
    if (!IsWindowVisible(hwnd) || GetAncestor(hwnd, GA_ROOT) != hwnd)
        return false;

    const LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    const bool appWindow = exStyle & WS_EX_APPWINDOW;
    if ((exStyle & WS_EX_TOOLWINDOW) && !appWindow)
        return false;
    if (GetWindow(hwnd, GW_OWNER) && !appWindow)
        return false;
    if (isCloaked(hwnd) || GetWindowTextLengthW(hwnd) == 0)
        return false;

    static const QStringList shellClasses = {
        QStringLiteral("Progman"),
        QStringLiteral("WorkerW"),
        QStringLiteral("Shell_TrayWnd"),
        QStringLiteral("Shell_SecondaryTrayWnd"),
    };
    return !shellClasses.contains(windowClass(hwnd));
}

// UWP apps all run in ApplicationFrameHost.exe. The actual process owns
// the child window Windows.UI.Core.CoreWindow (missing for minimized apps).
DWORD uwpProcessId(HWND frame, DWORD framePid)
{
    struct Search { DWORD framePid; DWORD result; } search{framePid, 0};
    EnumChildWindows(frame, [](HWND child, LPARAM param) -> BOOL {
        auto *search = reinterpret_cast<Search *>(param);
        DWORD pid = 0;
        GetWindowThreadProcessId(child, &pid);
        if (pid != search->framePid) {
            search->result = pid;
            return FALSE;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&search));
    return search.result;
}

QList<LiveWindow> enumerateWindows()
{
    QList<HWND> handles;
    EnumWindows([](HWND hwnd, LPARAM param) -> BOOL {
        reinterpret_cast<QList<HWND> *>(param)->append(hwnd);
        return TRUE;
    }, reinterpret_cast<LPARAM>(&handles));

    const DWORD ownPid = GetCurrentProcessId();
    QList<LiveWindow> result;

    for (HWND hwnd : std::as_const(handles)) {
        if (!isApplicationWindow(hwnd))
            continue;

        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid == ownPid)
            continue;

        LiveWindow live;
        live.hwnd = hwnd;
        WindowInfo &info = live.info;
        info.className = windowClass(hwnd);

        if (info.className == QLatin1String("ApplicationFrameWindow")) {
            if (const DWORD appPid = uwpProcessId(hwnd, pid))
                pid = appPid;
        }
        info.processId = pid;
        info.executablePath = processPath(pid);
        info.processName = QFileInfo(info.executablePath).fileName();

        WINDOWPLACEMENT placement{};
        placement.length = sizeof(placement);
        if (GetWindowPlacement(hwnd, &placement)) {
            info.normalRect = toQRect(placement.rcNormalPosition);
            info.minPosition = QPoint(placement.ptMinPosition.x, placement.ptMinPosition.y);
            info.maxPosition = QPoint(placement.ptMaxPosition.x, placement.ptMaxPosition.y);
            info.placementFlags = placement.flags;
        }
        if (IsIconic(hwnd))
            info.state = WindowState::Minimized;
        else if (IsZoomed(hwnd))
            info.state = WindowState::Maximized;

        RECT rect{};
        if (GetWindowRect(hwnd, &rect))
            info.windowRect = toQRect(rect);

        MONITORINFOEXW monitor{};
        monitor.cbSize = sizeof(monitor);
        if (GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor)) {
            info.monitorName = QString::fromWCharArray(monitor.szDevice);
            info.monitorRect = toQRect(monitor.rcMonitor);
        }

        result.append(live);
    }

    // Assign instance numbers. Sorted by process ID and handle so the order
    // does not depend on the Z-order and stays the same on restore for
    // processes that keep running.
    std::stable_sort(result.begin(), result.end(), [](const LiveWindow &a, const LiveWindow &b) {
        if (a.info.matchKey() != b.info.matchKey())
            return a.info.matchKey() < b.info.matchKey();
        if (a.info.processId != b.info.processId)
            return a.info.processId < b.info.processId;
        return reinterpret_cast<quintptr>(a.hwnd) < reinterpret_cast<quintptr>(b.hwnd);
    });
    QHash<QString, int> counters;
    for (LiveWindow &live : result)
        live.info.instance = counters[live.info.matchKey()]++;

    return result;
}

QString describe(const WindowInfo &info)
{
    return QStringLiteral("%1 [%2]").arg(info.processName, QString::number(info.instance + 1));
}

bool applyPlacement(HWND hwnd, const WindowInfo &info, UINT showCmd)
{
    WINDOWPLACEMENT placement{};
    placement.length = sizeof(placement);
    placement.flags = info.placementFlags & WPF_RESTORETOMAXIMIZED;
    placement.showCmd = showCmd;
    placement.ptMinPosition = POINT{info.minPosition.x(), info.minPosition.y()};
    placement.ptMaxPosition = POINT{info.maxPosition.x(), info.maxPosition.y()};
    placement.rcNormalPosition = toRect(info.normalRect);
    return SetWindowPlacement(hwnd, &placement);
}

bool restoreWindow(HWND hwnd, const WindowInfo &info)
{
    // Show normal first so that a maximized window can also change
    // monitors.
    if (!applyPlacement(hwnd, info, SW_SHOWNOACTIVATE))
        return false;

    switch (info.state) {
    case WindowState::Maximized:
        return applyPlacement(hwnd, info, SW_SHOWMAXIMIZED);
    case WindowState::Minimized:
        return applyPlacement(hwnd, info, SW_SHOWMINNOACTIVE);
    case WindowState::Normal:
        break;
    }

    // Set the actual rectangle, which differs from rcNormalPosition e.g. for
    // snapped windows. Twice, because DPI-aware applications adjust their
    // size themselves when moved to a monitor with a different scaling.
    const QRect &rect = info.windowRect;
    const UINT flags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER;
    if (!SetWindowPos(hwnd, nullptr, rect.x(), rect.y(), rect.width(), rect.height(), flags))
        return false;
    SetWindowPos(hwnd, nullptr, rect.x(), rect.y(), rect.width(), rect.height(), flags);
    return true;
}

} // namespace

namespace WindowGeometry {

QList<WindowInfo> captureWindows()
{
    QList<WindowInfo> windows;
    for (const LiveWindow &live : enumerateWindows())
        windows.append(live.info);
    return windows;
}

QList<MonitorInfo> monitors()
{
    QList<MonitorInfo> result;
    EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR handle, HDC, LPRECT, LPARAM param) -> BOOL {
        MONITORINFOEXW monitor{};
        monitor.cbSize = sizeof(monitor);
        if (!GetMonitorInfoW(handle, &monitor))
            return TRUE;

        MonitorInfo info;
        info.name = QString::fromWCharArray(monitor.szDevice);
        info.geometry = toQRect(monitor.rcMonitor);
        info.workArea = toQRect(monitor.rcWork);
        info.primary = monitor.dwFlags & MONITORINFOF_PRIMARY;

        UINT dpiX = 0;
        UINT dpiY = 0;
        if (SUCCEEDED(GetDpiForMonitor(handle, MDT_EFFECTIVE_DPI, &dpiX, &dpiY)))
            info.dpi = int(dpiX);
        reinterpret_cast<QList<MonitorInfo> *>(param)->append(info);
        return TRUE;
    }, reinterpret_cast<LPARAM>(&result));
    return result;
}

bool isRemoteSession()
{
    return GetSystemMetrics(SM_REMOTESESSION) != 0;
}

QSet<quintptr> windowHandles()
{
    QSet<quintptr> handles;
    for (const LiveWindow &live : enumerateWindows())
        handles.insert(reinterpret_cast<quintptr>(live.hwnd));
    return handles;
}

namespace {

struct MatchResult
{
    QList<QPair<HWND, WindowInfo>> matches;
    QList<WindowInfo> missing;
};

// Assigns saved windows to open windows of the same application in instance
// order. Windows in excludedHandles are ignored.
MatchResult matchWindows(const QList<WindowInfo> &windows, const QSet<quintptr> &excludedHandles)
{
    MatchResult result;

    // Group open windows by application, in instance order
    QHash<QString, QList<LiveWindow>> available;
    for (const LiveWindow &live : enumerateWindows()) {
        if (!excludedHandles.contains(reinterpret_cast<quintptr>(live.hwnd)))
            available[live.info.matchKey()].append(live);
    }

    // Group saved windows the same way
    QHash<QString, QList<WindowInfo>> wanted;
    QStringList keys;
    for (const WindowInfo &info : windows) {
        if (!wanted.contains(info.matchKey()))
            keys.append(info.matchKey());
        wanted[info.matchKey()].append(info);
    }

    for (const QString &key : std::as_const(keys)) {
        QList<WindowInfo> saved = wanted.value(key);
        std::sort(saved.begin(), saved.end(), [](const WindowInfo &a, const WindowInfo &b) {
            return a.instance < b.instance;
        });
        QList<LiveWindow> candidates = available.value(key);

        // Assigned in instance order
        for (qsizetype i = 0; i < saved.size(); ++i) {
            if (candidates.isEmpty())
                result.missing.append(saved.at(i));
            else
                result.matches.append({candidates.takeFirst().hwnd, saved.at(i)});
        }
    }
    return result;
}

} // namespace

bool placeNewWindow(quintptr handle, const QList<WindowInfo> &windows)
{
    const HWND hwnd = reinterpret_cast<HWND>(handle);
    const QList<LiveWindow> live = enumerateWindows();
    const auto self = std::find_if(live.cbegin(), live.cend(),
                                   [&](const LiveWindow &window) { return window.hwnd == hwnd; });
    if (self == live.cend())
        return false;

    QList<WindowInfo> saved;
    for (const WindowInfo &info : windows) {
        if (info.matchKey() == self->info.matchKey())
            saved.append(info);
    }
    if (saved.isEmpty())
        return false;

    // Saved entries that the other open windows of the application occupy
    // are not available for the new window. If all are in use, the entry
    // marked as default geometry is used, without one the window is left alone.
    const MatchResult others = matchWindows(saved, {handle});
    if (others.missing.isEmpty()) {
        const auto fallback = std::find_if(saved.cbegin(), saved.cend(),
                                           [](const WindowInfo &info) { return info.defaultGeometry; });
        if (fallback == saved.cend() || IsHungAppWindow(hwnd))
            return false;
        return restoreWindow(hwnd, *fallback);
    }

    QSet<quintptr> allOthers;
    for (const LiveWindow &window : live) {
        if (window.hwnd != hwnd)
            allOthers.insert(reinterpret_cast<quintptr>(window.hwnd));
    }
    const MatchResult mine = matchWindows(others.missing, allOthers);
    if (mine.matches.isEmpty() || IsHungAppWindow(hwnd))
        return false;
    return restoreWindow(hwnd, mine.matches.first().second);
}

RestoreResult restore(const QList<WindowInfo> &windows, const QSet<quintptr> &excludedHandles)
{
    RestoreResult result;
    const MatchResult matched = matchWindows(windows, excludedHandles);
    const QList<QPair<HWND, WindowInfo>> &matches = matched.matches;

    result.missing = matched.missing;
    result.notFound = int(matched.missing.size());
    for (const WindowInfo &info : matched.missing) {
        result.messages.append(QCoreApplication::translate("WindowGeometry", "Not found: %1")
                                   .arg(describe(info)));
    }
    for (const auto &[hwnd, info] : std::as_const(matches)) {
        result.usedHandles.insert(reinterpret_cast<quintptr>(hwnd));
        if (IsHungAppWindow(hwnd)) {
            ++result.failed;
            result.failures.append(QCoreApplication::translate("WindowGeometry", "Not responding: %1")
                                       .arg(describe(info)));
            continue;
        }
        if (restoreWindow(hwnd, info)) {
            ++result.restored;
        } else {
            const DWORD error = GetLastError();
            // Usually ERROR_ACCESS_DENIED for windows of processes with
            // higher privileges (UIPI)
            ++result.failed;
            result.failures.append(QCoreApplication::translate("WindowGeometry", "Failed (error %1): %2")
                                       .arg(error)
                                       .arg(describe(info)));
        }
    }

    result.messages.append(result.failures);
    return result;
}

} // namespace WindowGeometry

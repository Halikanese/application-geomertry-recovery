// SPDX-License-Identifier: MIT
//
// Copyright (c) 2026 Thorsten Becker
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <algorithm>

#include <QCoreApplication>
#include <QDateTime>
#include <QList>
#include <QPoint>
#include <QRect>
#include <QString>

// State of a window, corresponds to WINDOWPLACEMENT::showCmd
enum class WindowState {
    Normal,
    Minimized,
    Maximized,
};

// Identifies an application window: the EXE and the window class
struct ApplicationIdentity
{
    QString executablePath;   // Full path of the EXE
    QString processName;      // File name of the EXE
    QString className;        // Window class

    // Key for matching on restore
    QString matchKey() const { return executablePath.toLower(); }
    // Key for grouping the saved windows into applications
    QString groupKey() const { return executablePath.toLower() + u'|' + className; }
};

// Geometry of one window (instance) of an application. Coordinates are
// physical pixels (the application is per-monitor DPI aware).
struct InstanceInfo
{
    int instance = 0;         // Sequence number among windows of the same application

    WindowState state = WindowState::Normal;
    QRect windowRect;         // Actual rectangle in screen coordinates (GetWindowRect)
    QRect normalRect;         // rcNormalPosition in workspace coordinates (WINDOWPLACEMENT)
    QPoint minPosition;       // ptMinPosition
    QPoint maxPosition;       // ptMaxPosition
    uint placementFlags = 0;  // WINDOWPLACEMENT::flags

    // Informational only
    QString monitorName;      // Device name of the monitor the window was on
    QRect monitorRect;
    qint64 processId = 0;

    // Used for new windows of the application when all saved entries are in
    // use; at most one entry per executable path
    bool defaultGeometry = false;

    // Start the application for this window when the profile is loaded and
    // the window is not open
    bool autostart = false;
};

// One window with everything needed to recognize and restore it
struct WindowInfo : ApplicationIdentity, InstanceInfo
{
};

// Saved application with its windows, as stored in a profile
struct ApplicationInfo : ApplicationIdentity
{
    QList<InstanceInfo> instances;
};
struct MonitorInfo
{
    QString name;             // Device name, e.g. \\.\DISPLAY1
    QRect geometry;           // Monitor rectangle in physical pixels
    QRect workArea;           // Work area without the taskbar
    int dpi = 96;
    bool primary = false;
};

struct Profile
{
    QString name;
    QDateTime created;
    bool remoteSession = false;   // Connected via RDP when saved
    QString computerName;
    QList<MonitorInfo> monitors;
    QList<ApplicationInfo> applications;

    // All windows as a flat list
    QList<WindowInfo> windows() const
    {
        QList<WindowInfo> result;
        for (const ApplicationInfo &application : applications) {
            for (const InstanceInfo &instance : application.instances) {
                WindowInfo window;
                static_cast<ApplicationIdentity &>(window) = application;
                static_cast<InstanceInfo &>(window) = instance;
                result.append(window);
            }
        }
        return result;
    }

    // Groups the windows by EXE and window class, keeping their order
    void setWindows(const QList<WindowInfo> &windows)
    {
        applications.clear();
        for (const WindowInfo &window : windows) {
            auto it = std::find_if(applications.begin(), applications.end(),
                                   [&](const ApplicationInfo &application) {
                                       return application.groupKey() == window.groupKey();
                                   });
            if (it == applications.end()) {
                ApplicationInfo application;
                static_cast<ApplicationIdentity &>(application) = window;
                applications.append(application);
                it = applications.end() - 1;
            }
            it->instances.append(window);
        }
    }

    int windowCount() const
    {
        int count = 0;
        for (const ApplicationInfo &application : applications)
            count += int(application.instances.size());
        return count;
    }
};

inline QString windowStateText(WindowState state)
{
    switch (state) {
    case WindowState::Minimized:
        return QCoreApplication::translate("WindowState", "Minimized");
    case WindowState::Maximized:
        return QCoreApplication::translate("WindowState", "Maximized");
    case WindowState::Normal:
        break;
    }
    return QCoreApplication::translate("WindowState", "Normal");
}

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

#include "WindowInfo.h"

#include <QSet>
#include <QStringList>

// Access to the top-level windows of the Windows desktop
namespace WindowGeometry {

struct RestoreResult
{
    int restored = 0;
    int notFound = 0;
    int failed = 0;
    QStringList messages;     // Details on windows not found or failed
    QStringList failures;     // Only the failed ones of messages
    QList<WindowInfo> missing; // Saved windows without an open window
    QSet<quintptr> usedHandles; // Open windows that were matched
};

// All visible application windows (as in the taskbar), except our own
QList<WindowInfo> captureWindows();

// Current monitor configuration
QList<MonitorInfo> monitors();

// true if the session is connected via Remote Desktop
bool isRemoteSession();

// Matches the saved windows to the currently open ones and restores their
// geometry. With multiple instances of an application, windows are matched
// in instance order. Windows in excludedHandles are ignored.
RestoreResult restore(const QList<WindowInfo> &windows,
                      const QSet<quintptr> &excludedHandles = {});

// Handles of all application windows that are currently open
QSet<quintptr> windowHandles();

// Places a newly opened window according to the saved windows of its
// application, using an entry that no other open window occupies, or the
// entry marked as default geometry if all are in use. Returns false if the
// window is not an application window or there is no entry for it.
bool placeNewWindow(quintptr handle, const QList<WindowInfo> &windows);

} // namespace WindowGeometry

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

#include <QObject>
#include <QSet>

// Reports top-level windows of other processes that are shown for the first
// time, e.g. after an application was started. Uses a Windows event hook
// (SetWinEventHook), only one instance may exist.
class WindowWatcher : public QObject
{
    Q_OBJECT

public:
    explicit WindowWatcher(QObject *parent = nullptr);
    ~WindowWatcher() override;

    // Called by the event hook
    static void handleEventForHook(quint32 event, quintptr handle);

signals:
    // Emitted with a delay, so that the application can finish setting up
    // the window (title, own position)
    void windowOpened(quintptr handle);

private:
    void handleEvent(quint32 event, quintptr handle);

    static WindowWatcher *s_instance;
    QSet<quintptr> m_knownWindows;
    void *m_showHook = nullptr;
    void *m_destroyHook = nullptr;
};

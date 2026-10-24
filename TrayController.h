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
#include <QPointer>

class QMenu;
class QSystemTrayIcon;
class MainWindow;
class ProfileStore;

// System tray icon with the menu items Open, Save and Load
class TrayController : public QObject
{
    Q_OBJECT

public:
    explicit TrayController(ProfileStore *store, QObject *parent = nullptr);
    ~TrayController() override;

    void show();

private:
    void openMainWindow();
    void saveProfile();
    void restoreProfile(const QString &name);
    void showResult(const QString &title, QString text, const QStringList &messages, bool warning);
    void rebuildLoadMenu();
    void placeNewWindow(quintptr handle);

    ProfileStore *m_store;
    QSystemTrayIcon *m_trayIcon;
    QMenu *m_menu;
    QMenu *m_loadMenu;
    QPointer<MainWindow> m_mainWindow;
};

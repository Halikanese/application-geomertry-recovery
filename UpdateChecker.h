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

#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QUrl>

class QWidget;

// Looks for a newer release on GitHub (releases/latest of the repository set
// with the CMake variable AGR_UPDATE_REPOSITORY). If there is one, the setup
// program of the release is downloaded to the temp folder and started, and
// the application quits so that it can be replaced.
class UpdateChecker : public QObject
{
    Q_OBJECT

public:
    // window: parent of the dialogs
    explicit UpdateChecker(QWidget *window, QObject *parent = nullptr);

    // Repository as "owner/name", empty if none is configured
    static QString repository();

    void check();

private:
    void handleRelease(QNetworkReply *reply);
    void download(const QUrl &url, const QString &fileName);
    void install(const QString &installer);

    QNetworkAccessManager m_network;
    QPointer<QWidget> m_window;
    bool m_busy = false;
};

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
#include <QVariantList>

class ProfileModel;

// Provides the monitors and windows of the profile shown in a ProfileModel to
// the layout view (LayoutView.qml) and writes geometry changes back. Available
// in QML as "layoutBridge".
class LayoutBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList monitors READ monitors NOTIFY layoutChanged)
    Q_PROPERTY(QVariantList windows READ windows NOTIFY layoutChanged)
    Q_PROPERTY(int selectedApplication READ selectedApplication NOTIFY selectionChanged)
    Q_PROPERTY(int selectedInstance READ selectedInstance NOTIFY selectionChanged)

public:
    explicit LayoutBridge(ProfileModel *model, QObject *parent = nullptr);

    // Monitors: name, x, y, width, height, primary and the work area saved in
    // the profile as workX, workY, workWidth, workHeight (physical pixels)
    QVariantList monitors() const;
    // Windows: application, instance (rows in the model), name, number,
    // x, y, width, height, color
    QVariantList windows() const;

    int selectedApplication() const { return m_selectedApplication; }
    int selectedInstance() const { return m_selectedInstance; }
    // Selection in the window list, -1 for none
    void setSelection(int applicationRow, int instanceRow);

    // Called from QML
    Q_INVOKABLE void select(int applicationRow, int instanceRow);
    Q_INVOKABLE void setWindowRect(int applicationRow, int instanceRow, int x, int y, int width, int height);
    Q_INVOKABLE void finishEdit();

signals:
    void layoutChanged();
    void selectionChanged();
    // A window was clicked in the layout view
    void selectionRequested(int applicationRow, int instanceRow);

private:
    ProfileModel *m_model;
    int m_selectedApplication = -1;
    int m_selectedInstance = -1;
};

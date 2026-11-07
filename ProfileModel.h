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

#include <QAbstractItemModel>

// Saved applications of a profile as a tree: on the top level the
// applications (EXE), below them their windows (instances). Editable are the
// check boxes of the instances: "Default geometry", "Autostart" and the
// matching options in the "Instance", "Window class" and "Title" columns.
class ProfileModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum Column {
        ApplicationColumn,
        DefaultColumn,
        AutostartColumn,
        InstanceColumn,
        StateColumn,
        WindowRectColumn,
        NormalRectColumn,
        MonitorColumn,
        ClassColumn,
        TitleColumn,
        PathColumn,
        ColumnCount,
    };

    explicit ProfileModel(QObject *parent = nullptr);

    const Profile &profile() const { return m_profile; }
    void setProfile(const Profile &profile);

    // Removes an instance or, on the top level, the whole application
    void removeEntry(const QModelIndex &index);

    // Rectangle shown and edited in the layout view: the window rectangle
    // for normal windows, the restored (normal) position otherwise
    static QRect editableRect(const InstanceInfo &instance);
    // Changes the geometry of an instance without saving it yet
    void setInstanceRect(int applicationRow, int instanceRow, const QRect &rect);
    // Reports the changes made with setInstanceRect as profileEdited()
    void finishGeometryEdit();
    QModelIndex instanceIndex(int applicationRow, int instanceRow, int column = 0) const;

    QModelIndex index(int row, int column, const QModelIndex &parent = {}) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

signals:
    // The user changed the profile (check boxes, removed entries)
    void profileEdited();

private:
    QVariant applicationData(const ApplicationInfo &application, int column, int role) const;
    QVariant instanceData(const InstanceInfo &instance, int column, int role) const;

    Profile m_profile;
};

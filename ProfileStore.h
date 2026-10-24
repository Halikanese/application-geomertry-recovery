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

#include <QObject>
#include <QStringList>

// Saved profiles in an XML file (QSettings with XmlSettingsBackend).
// All profiles are kept in memory and written back completely on every
// change.
class ProfileStore : public QObject
{
    Q_OBJECT

public:
    explicit ProfileStore(const QString &fileName, QObject *parent = nullptr);

    QString fileName() const { return m_fileName; }

    QStringList names() const;
    bool contains(const QString &name) const;
    Profile profile(const QString &name) const;

    // Creates the profile or overwrites an existing one with the same name
    void store(const Profile &profile);
    void remove(const QString &name);

    // Profile that was loaded last, its windows are placed when they open
    QString activeProfile() const { return contains(m_activeProfile) ? m_activeProfile : QString(); }
    void setActiveProfile(const QString &name);

signals:
    void profilesChanged();
    void activeProfileChanged();

private:
    void load();
    void save() const;

    QString m_fileName;
    QList<Profile> m_profiles;
    QString m_activeProfile;
};

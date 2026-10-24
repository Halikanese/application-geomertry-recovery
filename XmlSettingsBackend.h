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

#include <QSettings>

class QXmlStreamReader;
class QXmlStreamWriter;

// XML format for QSettings. Groups become <group> elements, values become
// <value> elements with a type attribute, so that they get their original
// type back when read:
//
//   <settings>
//     <group name="display">
//       <value name="timeout" type="int">30</value>
//     </group>
//   </settings>
//
// Usage:
//   QSettings settings(path, XmlSettingsBackend::format());
// or for all QSettings without an explicit format:
//   QSettings::setDefaultFormat(XmlSettingsBackend::format());
class XmlSettingsBackend
{
public:
    // Registers the format on the first call and returns it.
    static QSettings::Format format();

private:
    static bool read(QIODevice &device, QSettings::SettingsMap &map);
    static bool write(QIODevice &device, const QSettings::SettingsMap &map);

    static void readGroup(QXmlStreamReader &reader, const QString &prefix,
                          QSettings::SettingsMap &map);
    static QVariant readVariant(QXmlStreamReader &reader);
    static void writeVariant(QXmlStreamWriter &writer, const QString &element,
                             const QString &name, const QVariant &value);
};

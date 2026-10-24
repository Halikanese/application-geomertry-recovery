#include "XmlSettingsBackend.h"

#include <algorithm>

#include <QDataStream>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

static const QString rootElement = QStringLiteral("settings");
static const QString groupElement = QStringLiteral("group");
static const QString valueElement = QStringLiteral("value");
static const QString itemElement = QStringLiteral("item");
static const QString entryElement = QStringLiteral("entry");
static const QString nameAttribute = QStringLiteral("name");
static const QString typeAttribute = QStringLiteral("type");

// Type names for special cases that are not stored directly as text
static const QString invalidType = QStringLiteral("invalid");
static const QString binaryType = QStringLiteral("binary");

// Control characters (except tab and newline) are not allowed in XML 1.0,
// \r would be normalized to \n when reading.
static bool isXmlSafe(const QString &text)
{
    for (const QChar c : text) {
        if (c.unicode() < 0x20 && c != u'\t' && c != u'\n')
            return false;
    }
    return true;
}

static bool canStoreAsText(const QVariant &value)
{
    const QMetaType type = value.metaType();
    if (!QMetaType::canConvert(type, QMetaType::fromType<QString>())
        || !QMetaType::canConvert(QMetaType::fromType<QString>(), type))
        return false;
    return isXmlSafe(value.toString());
}

QSettings::Format XmlSettingsBackend::format()
{
    static const QSettings::Format xmlFormat =
        QSettings::registerFormat(QStringLiteral("xml"), &XmlSettingsBackend::read,
                                  &XmlSettingsBackend::write);
    return xmlFormat;
}

bool XmlSettingsBackend::read(QIODevice &device, QSettings::SettingsMap &map)
{
    QXmlStreamReader reader(&device);
    if (!reader.readNextStartElement() || reader.name() != rootElement)
        return false;

    readGroup(reader, QString(), map);
    return !reader.hasError();
}

void XmlSettingsBackend::readGroup(QXmlStreamReader &reader, const QString &prefix,
                                   QSettings::SettingsMap &map)
{
    while (reader.readNextStartElement()) {
        const QString name = reader.attributes().value(nameAttribute).toString();
        if (reader.name() == groupElement)
            readGroup(reader, prefix + name + u'/', map);
        else if (reader.name() == valueElement)
            map.insert(prefix + name, readVariant(reader));
        else
            reader.skipCurrentElement();
    }
}

QVariant XmlSettingsBackend::readVariant(QXmlStreamReader &reader)
{
    const QString type = reader.attributes().value(typeAttribute).toString();

    if (type == invalidType) {
        reader.skipCurrentElement();
        return QVariant();
    }

    if (type == QLatin1String("QStringList")) {
        QStringList list;
        while (reader.readNextStartElement()) {
            if (reader.name() == itemElement)
                list.append(reader.readElementText());
            else
                reader.skipCurrentElement();
        }
        return list;
    }

    if (type == QLatin1String("QVariantList")) {
        QVariantList list;
        while (reader.readNextStartElement()) {
            if (reader.name() == itemElement)
                list.append(readVariant(reader));
            else
                reader.skipCurrentElement();
        }
        return list;
    }

    if (type == QLatin1String("QVariantMap")) {
        QVariantMap map;
        while (reader.readNextStartElement()) {
            if (reader.name() == entryElement) {
                const QString key = reader.attributes().value(nameAttribute).toString();
                map.insert(key, readVariant(reader));
            } else {
                reader.skipCurrentElement();
            }
        }
        return map;
    }

    const QString text = reader.readElementText();

    if (type == QLatin1String("QByteArray"))
        return QByteArray::fromBase64(text.toLatin1());

    if (type == binaryType) {
        QByteArray data = QByteArray::fromBase64(text.toLatin1());
        QDataStream stream(&data, QIODevice::ReadOnly);
        stream.setVersion(QDataStream::Qt_6_0);
        QVariant value;
        stream >> value;
        return value;
    }

    // Unknown type or conversion fails: return the text unchanged
    const QMetaType metaType = QMetaType::fromName(type.toLatin1());
    QVariant value(text);
    if (metaType.isValid() && value.convert(metaType))
        return value;
    return text;
}

bool XmlSettingsBackend::write(QIODevice &device, const QSettings::SettingsMap &map)
{
    QXmlStreamWriter writer(&device);
    writer.setAutoFormatting(true);
    writer.writeStartDocument();
    writer.writeStartElement(rootElement);

    // The keys are sorted, so keys of a group follow each other directly.
    // Only the groups that differ from the previous key are
    // switched.
    QStringList openGroups;
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
        QStringList groups = it.key().split(u'/', Qt::SkipEmptyParts);
        if (groups.isEmpty())
            continue;
        const QString name = groups.takeLast();

        qsizetype common = 0;
        while (common < openGroups.size() && common < groups.size()
               && openGroups.at(common) == groups.at(common))
            ++common;

        while (openGroups.size() > common) {
            writer.writeEndElement();
            openGroups.removeLast();
        }
        while (openGroups.size() < groups.size()) {
            const QString &group = groups.at(openGroups.size());
            writer.writeStartElement(groupElement);
            writer.writeAttribute(nameAttribute, group);
            openGroups.append(group);
        }

        writeVariant(writer, valueElement, name, it.value());
    }

    writer.writeEndDocument();
    return !writer.hasError();
}

void XmlSettingsBackend::writeVariant(QXmlStreamWriter &writer, const QString &element,
                                      const QString &name, const QVariant &value)
{
    writer.writeStartElement(element);
    if (!name.isNull())
        writer.writeAttribute(nameAttribute, name);

    const int typeId = value.typeId();
    const QStringList stringList =
        typeId == QMetaType::QStringList ? value.toStringList() : QStringList();

    if (typeId == QMetaType::UnknownType) {
        writer.writeAttribute(typeAttribute, invalidType);
    } else if (typeId == QMetaType::QStringList
               && std::all_of(stringList.cbegin(), stringList.cend(), isXmlSafe)) {
        writer.writeAttribute(typeAttribute, QStringLiteral("QStringList"));
        for (const QString &item : stringList)
            writer.writeTextElement(itemElement, item);
    } else if (typeId == QMetaType::QVariantList) {
        writer.writeAttribute(typeAttribute, QStringLiteral("QVariantList"));
        for (const QVariant &item : value.toList())
            writeVariant(writer, itemElement, QString(), item);
    } else if (typeId == QMetaType::QVariantMap || typeId == QMetaType::QVariantHash) {
        writer.writeAttribute(typeAttribute, QStringLiteral("QVariantMap"));
        const QVariantMap map = value.toMap();
        for (auto it = map.constBegin(); it != map.constEnd(); ++it)
            writeVariant(writer, entryElement, it.key(), it.value());
    } else if (typeId == QMetaType::QByteArray) {
        writer.writeAttribute(typeAttribute, QStringLiteral("QByteArray"));
        writer.writeCharacters(QString::fromLatin1(value.toByteArray().toBase64()));
    } else if (typeId != QMetaType::QStringList && canStoreAsText(value)) {
        writer.writeAttribute(typeAttribute, QString::fromLatin1(value.metaType().name()));
        writer.writeCharacters(value.toString());
    } else {
        // Everything else (e.g. QRect, QSize) in binary via QDataStream
        QByteArray data;
        QDataStream stream(&data, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_6_0);
        stream << value;
        writer.writeAttribute(typeAttribute, binaryType);
        writer.writeCharacters(QString::fromLatin1(data.toBase64()));
    }

    writer.writeEndElement();
}

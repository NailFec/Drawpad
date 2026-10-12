// SPDX-License-Identifier: Apache-2.0
// Copyright NailFec 2026 <nailfec17@gmail.com>

#include "document_file.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>

#include <cmath>
#include <cstdint>

namespace drawpad {
namespace {

QString formatDouble(double value)
{
    if (!std::isfinite(value))
        return QStringLiteral("0");
    return QLocale::c().toString(value, 'g', QLocale::FloatingPointShortest);
}

QString formatPoint(PointD point)
{
    return QStringLiteral("[") + formatDouble(point.x) + QStringLiteral(",") + formatDouble(point.y)
        + QStringLiteral("]");
}

bool readPoint(const QJsonValue &value, PointD &point)
{
    const QJsonArray pair = value.toArray();
    if (pair.size() != 2)
        return false;
    point = {pair.at(0).toDouble(), pair.at(1).toDouble()};
    return std::isfinite(point.x) && std::isfinite(point.y);
}

} // namespace

QByteArray saveDocumentToJson(const StrokeDocument &document)
{
    QString json = QStringLiteral("{\n  \"format\": \"drawpad\",\n  \"version\": 1,\n  \"strokes\": [");
    bool firstStroke = true;
    for (const Stroke &stroke : document.frozenStrokes()) {
        if (!firstStroke)
            json += QLatin1Char(',');
        firstStroke = false;
        json += QStringLiteral("\n    {\"id\": ") + QString::number(stroke.id);
        json += QStringLiteral(", \"color\": \"") + stroke.color.name(QColor::HexRgb).toUpper() + QLatin1Char('"');
        json += QStringLiteral(", \"width\": ") + formatDouble(stroke.width);
        json += QStringLiteral(", \"tx\": ") + formatDouble(stroke.tx);
        json += QStringLiteral(", \"ty\": ") + formatDouble(stroke.ty);
        json += QStringLiteral(", \"cubics\": [");
        bool firstCubic = true;
        for (const Cubic &cubic : stroke.cubics) {
            if (!firstCubic)
                json += QLatin1Char(',');
            firstCubic = false;
            json += QLatin1Char('[') + formatPoint(cubic.p0) + QLatin1Char(',') + formatPoint(cubic.p1)
                + QLatin1Char(',') + formatPoint(cubic.p2) + QLatin1Char(',') + formatPoint(cubic.p3)
                + QLatin1Char(']');
        }
        json += QStringLiteral("]}");
    }
    if (!firstStroke)
        json += QLatin1Char('\n');
    json += QStringLiteral("  ]\n}\n");
    return json.toUtf8();
}

bool loadDocumentFromJson(StrokeDocument &document, const QByteArray &bytes, QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument json = QJsonDocument::fromJson(bytes, &parseError);
    if (!json.isObject()) {
        if (error)
            *error = QStringLiteral("The file is not a Drawpad document.");
        return false;
    }
    const QJsonObject root = json.object();
    if (root.value(QStringLiteral("format")).toString() != QLatin1String("drawpad")
        || root.value(QStringLiteral("version")).toInt() != 1) {
        if (error)
            *error = QStringLiteral("This Drawpad file version is not supported.");
        return false;
    }

    StrokeDocument loaded;
    const QJsonArray strokes = root.value(QStringLiteral("strokes")).toArray();
    for (const QJsonValue &value : strokes) {
        const QJsonObject object = value.toObject();
        const double idValue = object.value(QStringLiteral("id")).toDouble();
        if (idValue < 1 || idValue > static_cast<double>(UINT64_C(0x1fffffffffffff)))
            continue;
        const auto id = static_cast<std::uint64_t>(idValue);
        QColor color(object.value(QStringLiteral("color")).toString());
        if (!color.isValid())
            color = Qt::black;
        const double width = object.value(QStringLiteral("width")).toDouble(1);
        const double tx = object.value(QStringLiteral("tx")).toDouble();
        const double ty = object.value(QStringLiteral("ty")).toDouble();
        std::vector<Cubic> cubics;
        for (const QJsonValue &cubicValue : object.value(QStringLiteral("cubics")).toArray()) {
            const QJsonArray points = cubicValue.toArray();
            if (points.size() != 4)
                continue;
            Cubic cubic;
            if (!readPoint(points.at(0), cubic.p0) || !readPoint(points.at(1), cubic.p1)
                || !readPoint(points.at(2), cubic.p2) || !readPoint(points.at(3), cubic.p3))
                continue;
            cubics.push_back(cubic);
        }
        loaded.insertFrozen(id, color, std::isfinite(width) ? width : 1, std::move(cubics), tx, ty);
    }
    document.swap(loaded);
    return true;
}

bool saveDocument(const StrokeDocument &document, const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error)
            *error = QStringLiteral("Could not write the Drawpad file.");
        return false;
    }
    if (file.write(saveDocumentToJson(document)) < 0) {
        if (error)
            *error = QStringLiteral("Could not write the Drawpad file.");
        return false;
    }
    return true;
}

bool loadDocument(StrokeDocument &document, const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("Could not read the Drawpad file.");
        return false;
    }
    return loadDocumentFromJson(document, file.readAll(), error);
}

} // namespace drawpad

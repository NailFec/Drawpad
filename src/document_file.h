// SPDX-License-Identifier: Apache-2.0
// Copyright NailFec 2026 <nailfec17@gmail.com>

#pragma once

#include "stroke_document.h"

#include <QString>

class QByteArray;

namespace drawpad {

QByteArray saveDocumentToJson(const StrokeDocument &document);
bool loadDocumentFromJson(StrokeDocument &document, const QByteArray &bytes, QString *error);
bool saveDocument(const StrokeDocument &document, const QString &path, QString *error);
bool loadDocument(StrokeDocument &document, const QString &path, QString *error);

} // namespace drawpad

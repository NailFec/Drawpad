// SPDX-License-Identifier: Apache-2.0
// Copyright NailFec 2026 <nailfec17@gmail.com>

#include "mapping.h"

#include <cmath>

namespace drawpad {

QRectF fitTopLeft(QSizeF content, double aspect)
{
    if (!(content.width() > 0) || !(content.height() > 0))
        return {};
    if (!(aspect > 0) || !std::isfinite(aspect))
        return QRectF(0, 0, content.width(), content.height());

    double width = content.width();
    double height = width / aspect;
    if (height > content.height()) {
        height = content.height();
        width = height * aspect;
    }
    return QRectF(0, 0, width, height);
}

} // namespace drawpad

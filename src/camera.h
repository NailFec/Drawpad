// SPDX-License-Identifier: Apache-2.0
// Copyright NailFec 2026 <nailfec17@gmail.com>

#pragma once

#include "geometry.h"

#include <QPointF>
#include <QRectF>

namespace drawpad {

struct Camera {
    static constexpr double kMinScale = 1e-8;
    static constexpr double kMaxScale = 1e8;
    static constexpr double kOriginLimit = 1e12;

    double originX = 0;
    double originY = 0;
    double scale = 1;

    void clamp();
    void applyResize(double newFitWidth, double oldFitWidth);
    PointD toCanvas(QPointF screen, const QRectF &fit) const;
    QPointF toScreen(PointD canvas, const QRectF &fit) const;
};

} // namespace drawpad

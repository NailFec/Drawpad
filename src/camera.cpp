// SPDX-License-Identifier: Apache-2.0
// Copyright NailFec 2026 <nailfec17@gmail.com>

#include "camera.h"

#include <algorithm>
#include <cmath>

namespace drawpad {

void Camera::clamp()
{
    if (!std::isfinite(scale))
        scale = 1;
    scale = std::clamp(scale, kMinScale, kMaxScale);
    if (!std::isfinite(originX))
        originX = 0;
    if (!std::isfinite(originY))
        originY = 0;
    originX = std::clamp(originX, -kOriginLimit, kOriginLimit);
    originY = std::clamp(originY, -kOriginLimit, kOriginLimit);
}

void Camera::applyResize(double newFitWidth, double oldFitWidth)
{
    if (!(oldFitWidth > 0) || !(newFitWidth > 0) || newFitWidth == oldFitWidth)
        return;
    scale *= newFitWidth / oldFitWidth;
    clamp();
}

PointD Camera::toCanvas(QPointF screen, const QRectF &fit) const
{
    const double safeScale = scale == 0 ? 1 : scale;
    return {originX + (screen.x() - fit.left()) / safeScale,
            originY + (screen.y() - fit.top()) / safeScale};
}

QPointF Camera::toScreen(PointD canvas, const QRectF &fit) const
{
    return QPointF(fit.left() + (canvas.x - originX) * scale,
                   fit.top() + (canvas.y - originY) * scale);
}

} // namespace drawpad

// SPDX-License-Identifier: Apache-2.0
// Copyright NailFec 2026 <nailfec17@gmail.com>

#pragma once

#include "camera.h"

#include <QRectF>
#include <vector>

namespace drawpad {

enum class Tool { Pen, Eraser, Select, None };

struct Contact {
    int id = 0;
    double nx = 0;
    double ny = 0;
};

struct GestureResult {
    bool cancelActive = false;
    bool commitInk = false;
    bool finishLasso = false;
    bool inkPoint = false;
    bool lassoPoint = false;
    bool move = false;
    PointD point;
    PointD moveDelta;
};

class GestureTracker {
public:
    void setTouchpadSize(double width, double height);
    void endGesture();
    void releaseOneFinger();

    GestureResult update(const std::vector<Contact> &contacts, Tool tool, Camera &camera,
                         const QRectF &fit, bool startsInSelection);

private:
    enum class Mode { Idle, Ink, Lasso, Move, Pan, PanZoom, Swallow };

    void applyTwoFingers(const Contact &first, const Contact &second, Camera &camera, const QRectF &fit);

    double m_width = 1;
    double m_height = 1;
    Mode m_mode = Mode::Idle;
    bool m_latched = false;
    bool m_haveMid = false;
    double m_latchDistance = 0;
    double m_latchScale = 1;
    QPointF m_previousMid;
    PointD m_lastCanvas;
};

} // namespace drawpad

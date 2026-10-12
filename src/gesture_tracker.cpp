// SPDX-License-Identifier: Apache-2.0
// Copyright NailFec 2026 <nailfec17@gmail.com>

#include "gesture_tracker.h"

#include <algorithm>
#include <cmath>

namespace drawpad {

void GestureTracker::setTouchpadSize(double width, double height)
{
    m_width = width > 0 ? width : 1;
    m_height = height > 0 ? height : 1;
}

void GestureTracker::endGesture()
{
    m_mode = Mode::Idle;
    m_latched = false;
    m_haveMid = false;
    m_latchDistance = 0;
}

void GestureTracker::applyTwoFingers(const Contact &first, const Contact &second, Camera &camera,
                                     const QRectF &fit)
{
    const double midX = (first.nx + second.nx) * 0.5;
    const double midY = (first.ny + second.ny) * 0.5;
    const QPointF mid(fit.left() + midX * fit.width(), fit.top() + midY * fit.height());
    const double separation = std::hypot((first.nx - second.nx) * m_width, (first.ny - second.ny) * m_height);
    const double threshold = 0.12 * std::min(m_width, m_height);

    if (!m_latched && separation > threshold) {
        m_latched = true;
        m_latchDistance = separation;
        m_latchScale = camera.scale;
        m_mode = Mode::PanZoom;
    }

    if (m_haveMid) {
        const QPointF delta = mid - m_previousMid;
        camera.originX -= delta.x() / camera.scale;
        camera.originY -= delta.y() / camera.scale;
        camera.clamp();
    }

    if (m_latched && m_latchDistance > 1e-9) {
        const PointD under = camera.toCanvas(mid, fit);
        camera.scale = m_latchScale * (separation / m_latchDistance);
        camera.clamp();
        camera.originX = under.x - (mid.x() - fit.left()) / camera.scale;
        camera.originY = under.y - (mid.y() - fit.top()) / camera.scale;
        camera.clamp();
    }

    m_previousMid = mid;
    m_haveMid = true;
}

GestureResult GestureTracker::update(const std::vector<Contact> &contacts, Tool tool, Camera &camera,
                                     const QRectF &fit, bool startsInSelection)
{
    GestureResult result;
    if (m_mode == Mode::Swallow) {
        if (contacts.empty())
            m_mode = Mode::Idle;
        return result;
    }

    if (contacts.size() >= 3) {
        if (m_mode == Mode::Ink || m_mode == Mode::Lasso || m_mode == Mode::Move)
            result.cancelActive = true;
        m_mode = Mode::Swallow;
        m_latched = false;
        m_haveMid = false;
        return result;
    }

    if (contacts.empty()) {
        if (m_mode == Mode::Ink)
            result.commitInk = true;
        else if (m_mode == Mode::Lasso)
            result.finishLasso = true;
        m_mode = Mode::Idle;
        m_latched = false;
        m_haveMid = false;
        return result;
    }

    if (contacts.size() == 2) {
        if (m_mode == Mode::Ink || m_mode == Mode::Lasso || m_mode == Mode::Move) {
            result.cancelActive = true;
            m_mode = Mode::Pan;
            m_latched = false;
            m_haveMid = false;
        } else if (m_mode == Mode::Idle) {
            m_mode = Mode::Pan;
        }
        applyTwoFingers(contacts[0], contacts[1], camera, fit);
        return result;
    }

    if (m_mode == Mode::Pan || m_mode == Mode::PanZoom) {
        m_mode = Mode::Swallow;
        m_latched = false;
        m_haveMid = false;
        return result;
    }

    const QPointF screen(fit.left() + contacts[0].nx * fit.width(), fit.top() + contacts[0].ny * fit.height());
    const PointD canvas = camera.toCanvas(screen, fit);
    if (m_mode == Mode::Idle) {
        if (tool == Tool::Select)
            m_mode = startsInSelection ? Mode::Move : Mode::Lasso;
        else
            m_mode = Mode::Ink;
        m_lastCanvas = canvas;
        if (m_mode == Mode::Move) {
            result.move = true;
            return result;
        }
        result.point = canvas;
        result.inkPoint = m_mode == Mode::Ink;
        result.lassoPoint = m_mode == Mode::Lasso;
        return result;
    }

    if (m_mode == Mode::Ink) {
        result.inkPoint = true;
        result.point = canvas;
        return result;
    }
    if (m_mode == Mode::Lasso) {
        result.lassoPoint = true;
        result.point = canvas;
        return result;
    }
    if (m_mode == Mode::Move) {
        result.move = true;
        result.moveDelta = canvas - m_lastCanvas;
        m_lastCanvas = canvas;
        return result;
    }
    return result;
}

} // namespace drawpad

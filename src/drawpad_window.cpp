// SPDX-License-Identifier: Apache-2.0
// Copyright NailFec 2026 <nailfec17@gmail.com>

#include "drawpad_window.h"

#include "mapping.h"

#include <QtCanvasPainter/qcanvaspainter.h>

#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QKeyEvent>
#include <QMessageBox>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QStandardPaths>

#include <algorithm>
#include <cmath>

namespace drawpad {
namespace {

QColor paletteColor(int index)
{
    static const QColor colors[] = {
        QColor(0x00, 0x00, 0x00),
        QColor(0xFF, 0xFF, 0xFF),
        QColor(0xE2, 0x3B, 0x3B),
        QColor(0xF0, 0x8A, 0x24),
        QColor(0x2F, 0x6F, 0xED),
        QColor(0x1F, 0x9D, 0x55),
    };
    return colors[std::clamp(index, 0, 5)];
}

RectD visibleRect(const Camera &camera, const QRectF &content, const QRectF &fit)
{
    const PointD topLeft = camera.toCanvas(content.topLeft(), fit);
    const PointD bottomRight = camera.toCanvas(content.bottomRight(), fit);
    const double left = std::min(topLeft.x, bottomRight.x);
    const double top = std::min(topLeft.y, bottomRight.y);
    return {left, top, std::abs(bottomRight.x - topLeft.x), std::abs(bottomRight.y - topLeft.y)};
}

void traceCubics(QCanvasPainter *painter, const Stroke &stroke, const std::vector<Cubic> &cubics,
                 const Camera &camera, const QRectF &fit, const RectD &view)
{
    bool started = false;
    for (const Cubic &cubic : cubics) {
        const Cubic world = cubic.translated(stroke.tx, stroke.ty);
        if (!world.controlBounds().expanded(stroke.width * 0.5).intersects(view)) {
            started = false;
            continue;
        }
        const QPointF p0 = camera.toScreen(world.p0, fit);
        if (cubicIsDot(world)) {
            if (started) {
                painter->stroke();
                started = false;
            }
            painter->beginPath();
            painter->circle(p0.x(), p0.y(), std::max(stroke.width * camera.scale * 0.5, 0.5));
            painter->fill();
            continue;
        }
        const QPointF p1 = camera.toScreen(world.p1, fit);
        const QPointF p2 = camera.toScreen(world.p2, fit);
        const QPointF p3 = camera.toScreen(world.p3, fit);
        if (!started) {
            painter->moveTo(p0.x(), p0.y());
            started = true;
        }
        painter->bezierCurveTo(p1.x(), p1.y(), p2.x(), p2.y(), p3.x(), p3.y());
    }
    if (started)
        painter->stroke();
}

void drawStroke(QCanvasPainter *painter, const Stroke &stroke, const std::vector<Cubic> &cubics,
                const Camera &camera, const QRectF &fit, const RectD &view, bool selected)
{
    const RectD bounds = stroke.worldBounds();
    if (!bounds.intersects(view))
        return;
    const double screenWidth = bounds.w * camera.scale;
    const double screenHeight = bounds.h * camera.scale;
    if (screenWidth < 1 && screenHeight < 1) {
        const QPointF center = camera.toScreen(bounds.center(), fit);
        painter->beginPath();
        painter->circle(center.x(), center.y(), 0.5);
        painter->setFillStyle(stroke.color);
        painter->fill();
        return;
    }

    painter->setLineCap(QCanvasPainter::LineCap::Round);
    painter->setLineJoin(QCanvasPainter::LineJoin::Round);
    if (selected) {
        painter->setStrokeStyle(QColor(0x4C, 0x8D, 0xFF));
        painter->setGlobalAlpha(0.85);
        painter->setLineWidth(stroke.width * camera.scale + 3);
        painter->beginPath();
        traceCubics(painter, stroke, cubics, camera, fit, view);
        painter->setGlobalAlpha(1);
    }
    painter->setStrokeStyle(stroke.color);
    painter->setFillStyle(stroke.color);
    painter->setLineWidth(std::max(stroke.width * camera.scale, 0.01));
    painter->beginPath();
    traceCubics(painter, stroke, cubics, camera, fit, view);
}

void drawDashedRect(QCanvasPainter *painter, const QRectF &rect)
{
    painter->beginPath();
    const auto dash = [&](double x0, double y0, double x1, double y1) {
        const double length = std::hypot(x1 - x0, y1 - y0);
        const double step = 4;
        for (double offset = 0; offset < length; offset += step * 2) {
            const double start = offset / length;
            const double end = std::min(offset + step, length) / length;
            painter->moveTo(x0 + (x1 - x0) * start, y0 + (y1 - y0) * start);
            painter->lineTo(x0 + (x1 - x0) * end, y0 + (y1 - y0) * end);
        }
    };
    dash(rect.left(), rect.top(), rect.right(), rect.top());
    dash(rect.right(), rect.top(), rect.right(), rect.bottom());
    dash(rect.right(), rect.bottom(), rect.left(), rect.bottom());
    dash(rect.left(), rect.bottom(), rect.left(), rect.top());
    painter->stroke();
}

} // namespace

DrawpadWindow::DrawpadWindow(QRhi::Implementation api)
    : PainterWindow(api)
{
    setTitle(QStringLiteral("Drawpad"));
    resize(1280, 800);
    setMinimumSize(QSize(640, 480));
    setCursor(Qt::BlankCursor);
    connect(this, &QWindow::activeChanged, this, &DrawpadWindow::onActiveChanged);
    connect(&m_grabber, &TouchpadGrabber::contactsUpdated, this, &DrawpadWindow::onContacts);
    connect(&m_grabber, &TouchpadGrabber::touchpadsReady, this, &DrawpadWindow::onTouchpadsReady);
}

QRectF DrawpadWindow::contentRect() const
{
    return QRectF(0, 0, std::max<qreal>(0, width() - kToolbarWidth), std::max<qreal>(0, height()));
}

QRectF DrawpadWindow::buttonRect(int index) const
{
    constexpr qreal size = 64;
    constexpr qreal margin = 12;
    return QRectF(width() - kToolbarWidth + (kToolbarWidth - size) * 0.5, margin + index * (size + margin), size, size);
}

void DrawpadWindow::updateFit()
{
    const QRectF next = fitTopLeft(contentRect().size(), m_aspect);
    if (!m_fitReady) {
        m_fit = next;
        m_fitReady = next.width() > 0 && next.height() > 0;
        return;
    }
    m_camera.applyResize(next.width(), m_fit.width());
    m_fit = next;
}

void DrawpadWindow::resizeEvent(QResizeEvent *event)
{
    PainterWindow::resizeEvent(event);
    updateFit();
    requestUpdate();
}

void DrawpadWindow::onActiveChanged()
{
    m_grabber.setGrabbed(isActive());
    if (isActive())
        return;
    if (m_activeId) {
        m_document.commitStroke(m_activeId);
        m_activeId = 0;
    }
    if (!m_lasso.empty()) {
        m_selection = m_document.selectInside(m_lasso);
        m_lasso.clear();
    }
    m_eraserDown = false;
    m_toolLocked = false;
    m_tracker.endGesture();
    requestUpdate();
}

void DrawpadWindow::onTouchpadsReady(int count, double width, double height)
{
    m_touchpads = count > 0 ? Touchpads::Ready : Touchpads::Missing;
    if (count > 0 && width > 0 && height > 0) {
        m_aspect = width / height;
        m_tracker.setTouchpadSize(width, height);
        const QRectF next = fitTopLeft(contentRect().size(), m_aspect);
        const bool untouched = m_document.frozenStrokes().empty() && m_camera.scale == 1 && m_camera.originX == 0
            && m_camera.originY == 0;
        if (m_fitReady && !untouched)
            m_camera.applyResize(next.width(), m_fit.width());
        m_fit = next;
        m_fitReady = next.width() > 0 && next.height() > 0;
    }
    requestUpdate();
}

void DrawpadWindow::onContacts(const QVector<Contact> &contacts)
{
    if (!m_fitReady)
        updateFit();
    std::vector<Contact> fingers;
    fingers.reserve(static_cast<size_t>(contacts.size()));
    for (const Contact &contact : contacts)
        fingers.push_back(contact);

    bool startsInSelection = false;
    if (fingers.size() == 1 && m_tool == Tool::Select && !m_selection.empty()) {
        const QPointF screen(m_fit.left() + fingers[0].nx * m_fit.width(),
                             m_fit.top() + fingers[0].ny * m_fit.height());
        startsInSelection = m_document.boundsOf(m_selection).contains(m_camera.toCanvas(screen, m_fit));
    }
    applyGesture(m_tracker.update(fingers, m_tool, m_camera, m_fit, startsInSelection));
    requestUpdate();
}

void DrawpadWindow::applyGesture(const GestureResult &result)
{
    if (result.cancelActive) {
        if (m_activeId) {
            m_document.cancelStroke(m_activeId);
            m_activeId = 0;
        }
        m_lasso.clear();
        m_eraserDown = false;
        m_toolLocked = false;
    }

    const Tool activeTool = m_toolLocked ? m_lockedTool : m_tool;
    if (result.inkPoint) {
        if (!m_toolLocked) {
            m_lockedTool = m_tool;
            m_toolLocked = true;
        }
        if (activeTool == Tool::Pen) {
            if (!m_activeId)
                m_activeId = m_document.beginStroke(paletteColor(m_colorIndex), m_penWidth);
            m_document.addSample(m_activeId, result.point);
        } else if (activeTool == Tool::Eraser) {
            if (m_eraserDown)
                m_document.eraseAlong(m_eraserPoint, result.point, m_eraserWidth);
            m_eraserPoint = result.point;
            m_eraserDown = true;
            pruneSelection();
        }
    }
    if (result.lassoPoint)
        m_lasso.push_back(result.point);
    if (result.move)
        m_document.translate(m_selection, result.moveDelta.x, result.moveDelta.y);
    if (result.commitInk) {
        if (m_activeId)
            m_document.commitStroke(m_activeId);
        m_activeId = 0;
        m_eraserDown = false;
        m_toolLocked = false;
    }
    if (result.finishLasso) {
        m_selection = m_document.selectInside(m_lasso);
        m_lasso.clear();
    }
}

void DrawpadWindow::pruneSelection()
{
    std::vector<std::uint64_t> kept;
    for (std::uint64_t id : m_selection) {
        if (m_document.contains(id))
            kept.push_back(id);
    }
    m_selection = std::move(kept);
}

void DrawpadWindow::setTool(Tool next)
{
    m_tool = next;
    requestUpdate();
}

void DrawpadWindow::bumpWidth(int delta)
{
    if (m_tool == Tool::Select)
        return;
    double &width = m_tool == Tool::Pen ? m_penWidth : m_eraserWidth;
    width = std::clamp(width + delta, 1.0, 64.0);
    requestUpdate();
}

void DrawpadWindow::save()
{
    if (m_path.isEmpty()) {
        saveAs();
        return;
    }
    QString error;
    if (!saveDocument(m_document, m_path, &error))
        QMessageBox::warning(nullptr, QStringLiteral("Drawpad"), error);
}

void DrawpadWindow::saveAs()
{
    QString directory = m_path.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
        : QFileInfo(m_path).absolutePath();
    QString path = QFileDialog::getSaveFileName(nullptr, QStringLiteral("Save Drawpad"), directory,
                                                QStringLiteral("Drawpad (*.drawpad)"));
    if (path.isEmpty())
        return;
    if (!path.endsWith(QStringLiteral(".drawpad"), Qt::CaseInsensitive))
        path += QStringLiteral(".drawpad");
    QString error;
    if (!saveDocument(m_document, path, &error)) {
        QMessageBox::warning(nullptr, QStringLiteral("Drawpad"), error);
        return;
    }
    m_path = path;
}

void DrawpadWindow::open()
{
    const QString directory = m_path.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
        : QFileInfo(m_path).absolutePath();
    const QString path = QFileDialog::getOpenFileName(nullptr, QStringLiteral("Open Drawpad"), directory,
                                                      QStringLiteral("Drawpad (*.drawpad)"));
    if (path.isEmpty())
        return;
    QString error;
    if (!loadDocument(m_document, path, &error)) {
        QMessageBox::warning(nullptr, QStringLiteral("Drawpad"), error);
        return;
    }
    if (m_activeId) {
        m_document.cancelStroke(m_activeId);
        m_activeId = 0;
    }
    m_selection.clear();
    m_lasso.clear();
    m_eraserDown = false;
    m_path = path;
    requestUpdate();
}

void DrawpadWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        close();
        return;
    }
    const bool control = event->modifiers().testFlag(Qt::ControlModifier);
    const bool shift = event->modifiers().testFlag(Qt::ShiftModifier);
    if (control && event->key() == Qt::Key_S) {
        if (shift)
            saveAs();
        else
            save();
        return;
    }
    if (control && event->key() == Qt::Key_O) {
        open();
        return;
    }
    if (event->isAutoRepeat() && event->key() != Qt::Key_Plus && event->key() != Qt::Key_Minus)
        return;

    switch (event->key()) {
    case Qt::Key_P:
        setTool(Tool::Pen);
        break;
    case Qt::Key_E:
        setTool(Tool::Eraser);
        break;
    case Qt::Key_S:
        setTool(Tool::Select);
        break;
    case Qt::Key_Plus:
        bumpWidth(1);
        break;
    case Qt::Key_Minus:
        bumpWidth(-1);
        break;
    case Qt::Key_1:
    case Qt::Key_2:
    case Qt::Key_3:
    case Qt::Key_4:
    case Qt::Key_5:
    case Qt::Key_6:
        m_colorIndex = event->key() - Qt::Key_1;
        requestUpdate();
        break;
    default:
        PainterWindow::keyPressEvent(event);
        break;
    }
}

void DrawpadWindow::mousePressEvent(QMouseEvent *event)
{
    for (int index = 0; index < 3; ++index) {
        if (buttonRect(index).contains(event->position())) {
            setTool(static_cast<Tool>(index));
            return;
        }
    }
    PainterWindow::mousePressEvent(event);
}

void DrawpadWindow::paint(QCanvasPainter *painter)
{
    if (!m_fitReady)
        updateFit();

    const QRectF content = contentRect();
    painter->setFillStyle(QColor(0xE6, 0xE6, 0xE6));
    painter->beginPath();
    if (m_fit.right() < content.width() - 0.5)
        painter->rect(m_fit.right(), 0, content.width() - m_fit.right(), content.height());
    if (m_fit.bottom() < content.height() - 0.5)
        painter->rect(0, m_fit.bottom(), m_fit.width(), content.height() - m_fit.bottom());
    painter->fill();

    const RectD view = visibleRect(m_camera, content, m_fit);
    for (std::uint64_t id : m_document.query(view)) {
        const Stroke *stroke = m_document.stroke(id);
        if (!stroke || stroke->id == m_activeId)
            continue;
        const bool selected = std::find(m_selection.begin(), m_selection.end(), id) != m_selection.end();
        drawStroke(painter, *stroke, stroke->cubics, m_camera, m_fit, view, selected);
    }
    if (const Stroke *active = m_document.stroke(m_activeId)) {
        const std::vector<Cubic> preview = active->frozen ? active->cubics : cubicsFromSamples(active->samples);
        drawStroke(painter, *active, preview, m_camera, m_fit, view, false);
    }

    painter->setStrokeStyle(QColor(0xB4, 0xB4, 0xB4));
    painter->setLineWidth(1);
    painter->setLineCap(QCanvasPainter::LineCap::Butt);
    painter->beginPath();
    painter->rect(m_fit);
    painter->stroke();

    if (!m_lasso.empty()) {
        painter->setStrokeStyle(QColor(0x25, 0x63, 0xEB));
        painter->setLineWidth(1.5);
        painter->beginPath();
        const QPointF first = m_camera.toScreen(m_lasso.front(), m_fit);
        painter->moveTo(first.x(), first.y());
        for (size_t index = 1; index < m_lasso.size(); ++index) {
            const QPointF point = m_camera.toScreen(m_lasso[index], m_fit);
            painter->lineTo(point.x(), point.y());
        }
        painter->lineTo(first.x(), first.y());
        painter->stroke();
    }

    if (m_eraserDown) {
        const QPointF center = m_camera.toScreen(m_eraserPoint, m_fit);
        painter->setStrokeStyle(QColor(0x66, 0x66, 0x66));
        painter->setLineWidth(1);
        painter->beginPath();
        painter->circle(center.x(), center.y(), m_eraserWidth * m_camera.scale * 0.5);
        painter->stroke();
    }

    if (m_touchpads == Touchpads::Missing) {
        painter->setFillStyle(QColor(0x66, 0x66, 0x66));
        painter->setFont(QFont(QStringLiteral("sans-serif"), 16));
        painter->setTextAlign(QCanvasPainter::TextAlign::Center);
        painter->setTextBaseline(QCanvasPainter::TextBaseline::Middle);
        painter->fillText(QStringLiteral("Touchpad is not available"), content.center().x(), content.center().y());
    }

    painter->setFillStyle(QColor(0x26, 0x26, 0x26));
    painter->beginPath();
    painter->rect(content.width(), 0, kToolbarWidth, height());
    painter->fill();

    painter->setFont(QFont(QStringLiteral("sans-serif"), 13));
    painter->setTextAlign(QCanvasPainter::TextAlign::Center);
    painter->setTextBaseline(QCanvasPainter::TextBaseline::Middle);
    const char *labels[] = {"P", "E", "S"};
    for (int index = 0; index < 3; ++index) {
        const QRectF button = buttonRect(index);
        const bool active = static_cast<int>(m_tool) == index;
        painter->setFillStyle(active ? QColor(0x3F, 0x3F, 0x3F) : QColor(0x2E, 0x2E, 0x2E));
        painter->beginPath();
        painter->roundRect(button, 8);
        painter->fill();
        if (active) {
            painter->setStrokeStyle(QColor(0xFF, 0xFF, 0xFF));
            painter->setLineWidth(2);
            painter->beginPath();
            painter->roundRect(button.adjusted(1, 1, -1, -1), 8);
            painter->stroke();
        }

        painter->setFillStyle(QColor(0xFF, 0xFF, 0xFF));
        painter->fillText(QString::fromLatin1(labels[index]), button.center().x(), button.top() + 16);

        const QPointF center = button.center();
        if (index == 0) {
            painter->setStrokeStyle(paletteColor(m_colorIndex));
            painter->setLineWidth(3);
            painter->setLineCap(QCanvasPainter::LineCap::Round);
            painter->beginPath();
            painter->moveTo(center.x() - 12, center.y() + 8);
            painter->lineTo(center.x() + 12, center.y() - 10);
            painter->stroke();
        } else if (index == 1) {
            painter->setStrokeStyle(QColor(0xFF, 0xFF, 0xFF));
            painter->setLineWidth(2);
            painter->beginPath();
            painter->roundRect(center.x() - 12, center.y() - 8, 24, 16, 3);
            painter->stroke();
        } else {
            painter->setStrokeStyle(QColor(0xFF, 0xFF, 0xFF));
            painter->setLineWidth(1.5);
            drawDashedRect(painter, QRectF(center.x() - 12, center.y() - 8, 24, 16));
        }

        if (active && m_tool != Tool::Select) {
            const double width = m_tool == Tool::Pen ? m_penWidth : m_eraserWidth;
            painter->setFillStyle(QColor(0xFF, 0xFF, 0xFF));
            painter->fillText(QString::number(width, 'f', 0), button.center().x(), button.bottom() - 14);
        }
    }
}

} // namespace drawpad

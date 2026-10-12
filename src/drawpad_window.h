// SPDX-License-Identifier: Apache-2.0
// Copyright NailFec 2026 <nailfec17@gmail.com>

#pragma once

#include "document_file.h"
#include "painter_window.h"
#include "touchpad_grabber.h"

#include <QString>
#include <QVector>

#include <cstdint>
#include <vector>

namespace drawpad {

class DrawpadWindow : public PainterWindow {
    Q_OBJECT

public:
    explicit DrawpadWindow(QRhi::Implementation api);

protected:
    void paint(QCanvasPainter *painter) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    void onActiveChanged();
    void onContacts(const QVector<Contact> &contacts);
    void onTouchpadsReady(int count, double width, double height);
    void updateFit();
    void applyGesture(const GestureResult &result);
    void setTool(Tool next);
    void bumpWidth(int delta);
    void save();
    void saveAs();
    void open();
    void pruneSelection();
    QRectF contentRect() const;
    QRectF buttonRect(int index) const;

    static constexpr qreal kToolbarWidth = 88;

    StrokeDocument m_document;
    GestureTracker m_tracker;
    TouchpadGrabber m_grabber;
    Camera m_camera;
    QRectF m_fit;
    bool m_fitReady = false;
    double m_aspect = 1.6;
    Tool m_tool = Tool::Pen;
    int m_colorIndex = 0;
    double m_penWidth = 4;
    double m_eraserWidth = 16;
    std::uint64_t m_activeId = 0;
    bool m_eraserDown = false;
    PointD m_eraserPoint;
    bool m_toolLocked = false;
    Tool m_lockedTool = Tool::Pen;
    std::vector<PointD> m_lasso;
    std::vector<std::uint64_t> m_selection;
    QString m_path;
    enum class Touchpads { Unknown, Missing, Ready } m_touchpads = Touchpads::Unknown;
};

} // namespace drawpad

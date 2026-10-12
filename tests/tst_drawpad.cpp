// SPDX-License-Identifier: Apache-2.0
// Copyright NailFec 2026 <nailfec17@gmail.com>

#include "camera.h"
#include "document_file.h"
#include "geometry.h"
#include "gesture_tracker.h"
#include "mapping.h"
#include "stroke_document.h"

#include <QtTest>

#include <cmath>

using namespace drawpad;

namespace {

void expectNear(double actual, double expected)
{
    QVERIFY2(std::abs(actual - expected) < 1e-9, qPrintable(QString::number(actual, 'g', 17)));
}

Cubic line(PointD from, PointD to)
{
    const PointD delta = to - from;
    return {from, from + delta * (1.0 / 3.0), from + delta * (2.0 / 3.0), to};
}

} // namespace

class DrawpadTests : public QObject {
    Q_OBJECT

private slots:
    void fitRectLeavesTheExtraSpaceOnTheRightOrBottom();
    void resizeKeepsTheCanvasSpanUnderTheTouchpad();
    void cameraRelativePointStaysAccurateFarFromTheOrigin();
    void frozenCubicPassesThroughTheSamples();
    void translationDoesNotChangeControlPoints();
    void lassoSelectsOnlyStrokesFullyInside();
    void eraserDeletesTheTouchedSegmentOnly();
    void documentRoundTripsDoubles();
    void closeFingersPanAndSpreadFingersLatchZoom();
    void oneFingerWithoutAToolDoesNotInk();
};

void DrawpadTests::fitRectLeavesTheExtraSpaceOnTheRightOrBottom()
{
    const QRectF wide = fitTopLeft(QSizeF(200, 100), 1);
    QCOMPARE(wide, QRectF(0, 0, 100, 100));
    QVERIFY(wide.left() == 0);
    QVERIFY(wide.top() == 0);

    const QRectF tall = fitTopLeft(QSizeF(100, 200), 2);
    QCOMPARE(tall, QRectF(0, 0, 100, 50));
    QVERIFY(tall.height() < 200);
}

void DrawpadTests::resizeKeepsTheCanvasSpanUnderTheTouchpad()
{
    Camera camera;
    camera.scale = 2;
    camera.originX = 10;
    camera.originY = -4;
    camera.applyResize(150, 100);
    expectNear(camera.scale, 3);
    expectNear(camera.originX, 10);
    expectNear(150.0 / camera.scale, 100.0 / 2.0);

    camera.applyResize(150, 150);
    expectNear(camera.scale, 3);
}

void DrawpadTests::cameraRelativePointStaysAccurateFarFromTheOrigin()
{
    Camera camera;
    camera.originX = 1e9;
    camera.originY = -1e9;
    camera.scale = 2;
    const QRectF fit(10, 20, 100, 50);
    const QPointF screen = camera.toScreen(PointD {1e9 + 3, -1e9 + 4}, fit);
    expectNear(screen.x(), 16);
    expectNear(screen.y(), 28);

    const PointD canvas = camera.toCanvas(screen, fit);
    expectNear(canvas.x, 1e9 + 3);
    expectNear(canvas.y, -1e9 + 4);
}

void DrawpadTests::frozenCubicPassesThroughTheSamples()
{
    const std::vector<PointD> samples {{0, 0}, {10, 0}, {20, 5}};
    const std::vector<Cubic> first = cubicsFromSamples(samples);
    const std::vector<Cubic> second = cubicsFromSamples(samples);
    QCOMPARE(first.size(), second.size());
    QVERIFY(!first.empty());
    expectNear(first.front().p0.x, 0);
    expectNear(first.front().p0.y, 0);
    expectNear(first.back().p3.x, 20);
    expectNear(first.back().p3.y, 5);
    expectNear(first.front().p1.x, second.front().p1.x);
    expectNear(first.back().p2.y, second.back().p2.y);

    StrokeDocument document;
    const std::uint64_t id = document.beginStroke(Qt::black, 4);
    for (PointD sample : samples)
        document.addSample(id, sample);
    document.commitStroke(id);
    const Stroke *stroke = document.stroke(id);
    QVERIFY(stroke);
    QVERIFY(stroke->frozen);
    QVERIFY(stroke->samples.empty());
    expectNear(stroke->cubics.front().p0.x, 0);
    expectNear(stroke->cubics.back().p3.x, 20);
}

void DrawpadTests::translationDoesNotChangeControlPoints()
{
    StrokeDocument document;
    const Cubic cubic = line({0, 0}, {10, 4});
    const std::uint64_t id = document.insertFrozen(0, Qt::black, 4, {cubic}, 0, 0);
    document.translate({id}, 30, -8);
    const Stroke *stroke = document.stroke(id);
    QVERIFY(stroke);
    expectNear(stroke->cubics.front().p0.x, 0);
    expectNear(stroke->cubics.front().p0.y, 0);
    expectNear(stroke->cubics.front().p3.x, 10);
    expectNear(stroke->cubics.front().p3.y, 4);
    expectNear(stroke->tx, 30);
    expectNear(stroke->ty, -8);
}

void DrawpadTests::lassoSelectsOnlyStrokesFullyInside()
{
    StrokeDocument document;
    const std::uint64_t inside = document.insertFrozen(0, Qt::black, 2, {line({0, 0}, {10, 0})}, 0, 0);
    const std::uint64_t outside = document.insertFrozen(0, Qt::black, 2, {line({0, 0}, {10, 0})}, 0, 0);
    Q_UNUSED(outside);
    const std::vector<PointD> room {{-20, -20}, {40, -20}, {40, 40}, {-20, 40}};
    const std::vector<std::uint64_t> selected = document.selectInside(room);
    QCOMPARE(selected.size(), std::size_t {2});

    const std::vector<PointD> partial {{5, -20}, {40, -20}, {40, 40}, {5, 40}};
    QVERIFY(document.selectInside(partial).empty());
    QVERIFY(document.contains(inside));
}

void DrawpadTests::eraserDeletesTheTouchedSegmentOnly()
{
    StrokeDocument document;
    const Cubic kept = line({100, 0}, {110, 0});
    const std::uint64_t id = document.insertFrozen(0, Qt::red, 2, {line({0, 0}, {10, 0}), kept}, 0, 0);
    document.eraseAlong({5, 0}, {5, 0}, 4);
    const Stroke *stroke = document.stroke(id);
    QVERIFY(stroke);
    QCOMPARE(stroke->cubics.size(), std::size_t {1});
    expectNear(stroke->cubics.front().p0.x, kept.p0.x);
    expectNear(stroke->cubics.front().p1.y, kept.p1.y);
    expectNear(stroke->cubics.front().p2.x, kept.p2.x);
    expectNear(stroke->cubics.front().p3.y, kept.p3.y);
}

void DrawpadTests::documentRoundTripsDoubles()
{
    StrokeDocument document;
    const double coordinate = 1e12 + 0.25;
    document.insertFrozen(7, QColor(0x2F, 0x6F, 0xED), 4.5, {line({coordinate, -0.5}, {coordinate + 2, 3})},
                          coordinate, -coordinate);
    StrokeDocument loaded;
    QString error;
    QVERIFY2(loadDocumentFromJson(loaded, saveDocumentToJson(document), &error), qPrintable(error));
    const Stroke *stroke = loaded.stroke(7);
    QVERIFY(stroke);
    QCOMPARE(stroke->color, QColor(0x2F, 0x6F, 0xED));
    expectNear(stroke->width, 4.5);
    expectNear(stroke->tx, coordinate);
    expectNear(stroke->ty, -coordinate);
    expectNear(stroke->cubics.front().p0.x, coordinate);
    expectNear(stroke->cubics.front().p3.x, coordinate + 2);

    StrokeDocument untouched;
    untouched.insertFrozen(1, Qt::black, 1, {line({0, 0}, {1, 0})}, 0, 0);
    QVERIFY(!loadDocumentFromJson(untouched, QByteArray("{ \"format\": \"drawpad\", \"version\": 2 }"), &error));
    QVERIFY(untouched.contains(1));
}

void DrawpadTests::closeFingersPanAndSpreadFingersLatchZoom()
{
    GestureTracker tracker;
    tracker.setTouchpadSize(100, 64);
    Camera camera;
    const QRectF fit(0, 0, 200, 128);

    Contact left {1, 0.25, 0.5};
    Contact right {2, 0.25, 0.5625};
    tracker.update({left, right}, Tool::Pen, camera, fit, false);
    left.nx = 0.5;
    right.nx = 0.5;
    tracker.update({left, right}, Tool::Pen, camera, fit, false);
    expectNear(camera.scale, 1);
    expectNear(camera.originX, -50);

    right.ny = 0.75;
    tracker.update({left, right}, Tool::Pen, camera, fit, false);
    expectNear(camera.scale, 1);

    right.ny = 1;
    tracker.update({left, right}, Tool::Pen, camera, fit, false);
    expectNear(camera.scale, 2);

    right.ny = 0.5625;
    tracker.update({left, right}, Tool::Pen, camera, fit, false);
    expectNear(camera.scale, 0.25);

    GestureTracker ink;
    ink.setTouchpadSize(100, 64);
    const GestureResult started = ink.update({Contact {1, 0.25, 0.25}}, Tool::Pen, camera, fit, false);
    QVERIFY(started.inkPoint);
    const GestureResult cancelled =
        ink.update({Contact {1, 0.25, 0.25}, Contact {2, 0.5, 0.25}}, Tool::Pen, camera, fit, false);
    QVERIFY(cancelled.cancelActive);
    QVERIFY(!cancelled.commitInk);

    GestureTracker finished;
    finished.setTouchpadSize(100, 64);
    finished.update({Contact {1, 0.25, 0.25}}, Tool::Pen, camera, fit, false);
    const GestureResult committed = finished.update({}, Tool::Pen, camera, fit, false);
    QVERIFY(committed.commitInk);
}

void DrawpadTests::oneFingerWithoutAToolDoesNotInk()
{
    GestureTracker hover;
    hover.setTouchpadSize(100, 64);
    Camera camera;
    const QRectF fit(0, 0, 200, 128);
    const GestureResult idle = hover.update({Contact {1, 0.2, 0.3}}, Tool::None, camera, fit, false);
    QVERIFY(!idle.inkPoint);
    QVERIFY(!idle.lassoPoint);
    QVERIFY(!idle.commitInk);

    hover.releaseOneFinger();
    const GestureResult drew = hover.update({Contact {1, 0.2, 0.3}}, Tool::Pen, camera, fit, false);
    QVERIFY(drew.inkPoint);

    GestureTracker pan;
    pan.setTouchpadSize(100, 64);
    Camera panCamera;
    Contact left {1, 0.25, 0.5};
    Contact right {2, 0.25, 0.5625};
    pan.update({left}, Tool::None, panCamera, fit, false);
    pan.update({left, right}, Tool::None, panCamera, fit, false);
    left.nx = 0.5;
    right.nx = 0.5;
    pan.update({left, right}, Tool::None, panCamera, fit, false);
    expectNear(panCamera.scale, 1);
    expectNear(panCamera.originX, -50);
}

QTEST_GUILESS_MAIN(DrawpadTests)
#include "tst_drawpad.moc"

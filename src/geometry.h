// SPDX-License-Identifier: Apache-2.0
// Copyright NailFec 2026 <nailfec17@gmail.com>

#pragma once

#include <vector>

namespace drawpad {

struct PointD {
    double x = 0;
    double y = 0;

    PointD operator+(PointD other) const { return {x + other.x, y + other.y}; }
    PointD operator-(PointD other) const { return {x - other.x, y - other.y}; }
    PointD operator*(double scale) const { return {x * scale, y * scale}; }
};

struct RectD {
    double x = 0;
    double y = 0;
    double w = 0;
    double h = 0;

    bool isFinite() const;
    bool contains(PointD point) const;
    bool intersects(const RectD &other) const;
    bool containsRect(const RectD &inner) const;
    RectD united(const RectD &other) const;
    RectD expanded(double pad) const;
    PointD center() const { return {x + w * 0.5, y + h * 0.5}; }
};

struct Cubic {
    PointD p0;
    PointD p1;
    PointD p2;
    PointD p3;

    Cubic translated(double tx, double ty) const;
    RectD controlBounds() const;
};

double distance(PointD a, PointD b);
bool nearlyEqual(double a, double b, double epsilon = 1e-9);

RectD boundsOfSegment(PointD a, PointD b);
double pointSegmentDistance(PointD point, PointD a, PointD b);
double segmentDistance(PointD a, PointD b, PointD c, PointD d);
bool segmentsIntersect(PointD a, PointD b, PointD c, PointD d);

bool pointInPolygon(PointD point, const std::vector<PointD> &polygon);
double distanceToPolygonBoundary(PointD point, const std::vector<PointD> &polygon);
bool insideByMargin(PointD point, const std::vector<PointD> &polygon, double margin);
RectD polygonBounds(const std::vector<PointD> &polygon);

bool cubicHitsSegment(const Cubic &cubic, PointD a, PointD b, double radius);
bool cubicCrossesPolygon(const Cubic &cubic, const std::vector<PointD> &polygon);
bool cubicIsDot(const Cubic &cubic);

std::vector<Cubic> cubicsFromSamples(const std::vector<PointD> &samples);

} // namespace drawpad

// SPDX-License-Identifier: Apache-2.0
// Copyright NailFec 2026 <nailfec17@gmail.com>

#include "geometry.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace drawpad {
namespace {

constexpr double kFlatness = 0.25;
constexpr int kMaxDepth = 8;

bool finitePoint(PointD point)
{
    return std::isfinite(point.x) && std::isfinite(point.y);
}

int sideOf(PointD a, PointD b, PointD point)
{
    const double value = (b.x - a.x) * (point.y - a.y) - (b.y - a.y) * (point.x - a.x);
    if (value > 1e-9)
        return 1;
    if (value < -1e-9)
        return -1;
    return 0;
}

bool pointOnSegment(PointD a, PointD b, PointD point)
{
    return point.x >= std::min(a.x, b.x) - 1e-9 && point.x <= std::max(a.x, b.x) + 1e-9
        && point.y >= std::min(a.y, b.y) - 1e-9 && point.y <= std::max(a.y, b.y) + 1e-9;
}

double lineDistance(PointD point, PointD a, PointD b)
{
    const double length = distance(a, b);
    if (length < 1e-12)
        return distance(point, a);
    return std::abs((b.x - a.x) * (a.y - point.y) - (a.x - point.x) * (b.y - a.y)) / length;
}

bool isFlat(const Cubic &cubic)
{
    return lineDistance(cubic.p1, cubic.p0, cubic.p3) < kFlatness
        && lineDistance(cubic.p2, cubic.p0, cubic.p3) < kFlatness;
}

void splitCubic(const Cubic &cubic, Cubic &left, Cubic &right)
{
    const PointD p01 = (cubic.p0 + cubic.p1) * 0.5;
    const PointD p12 = (cubic.p1 + cubic.p2) * 0.5;
    const PointD p23 = (cubic.p2 + cubic.p3) * 0.5;
    const PointD p012 = (p01 + p12) * 0.5;
    const PointD p123 = (p12 + p23) * 0.5;
    const PointD mid = (p012 + p123) * 0.5;
    left = {cubic.p0, p01, p012, mid};
    right = {mid, p123, p23, cubic.p3};
}

PointD tangentAt(PointD a, PointD b, PointD c, double ta, double tb, double tc)
{
    const double d0 = std::max(tb - ta, 1e-12);
    const double d1 = std::max(tc - ta, 1e-12);
    const double d2 = std::max(tc - tb, 1e-12);
    return {(b.x - a.x) / d0 - (c.x - a.x) / d1 + (c.x - b.x) / d2,
            (b.y - a.y) / d0 - (c.y - a.y) / d1 + (c.y - b.y) / d2};
}

Cubic catmullSegment(PointD p0, PointD p1, PointD p2, PointD p3)
{
    constexpr double alpha = 0.5;
    const auto knot = [](double start, PointD from, PointD to) {
        return start + std::pow(std::max(distance(from, to), 1e-12), alpha);
    };
    const double t0 = 0;
    const double t1 = knot(t0, p0, p1);
    const double t2 = knot(t1, p1, p2);
    const double t3 = knot(t2, p2, p3);
    const double span = std::max(t2 - t1, 1e-12);
    const PointD m1 = tangentAt(p0, p1, p2, t0, t1, t2) * span;
    const PointD m2 = tangentAt(p1, p2, p3, t1, t2, t3) * span;
    return {p1, p1 + m1 * (1.0 / 3.0), p2 - m2 * (1.0 / 3.0), p2};
}

bool hitsSegment(const Cubic &cubic, PointD a, PointD b, double radius, int depth)
{
    if (!cubic.controlBounds().expanded(radius).intersects(boundsOfSegment(a, b).expanded(radius)))
        return false;
    if (depth >= kMaxDepth || isFlat(cubic))
        return segmentDistance(cubic.p0, cubic.p3, a, b) <= radius;
    Cubic left;
    Cubic right;
    splitCubic(cubic, left, right);
    return hitsSegment(left, a, b, radius, depth + 1) || hitsSegment(right, a, b, radius, depth + 1);
}

bool crossesPolygon(const Cubic &cubic, const std::vector<PointD> &polygon, int depth)
{
    if (!cubic.controlBounds().intersects(polygonBounds(polygon)))
        return false;
    if (depth >= kMaxDepth || isFlat(cubic)) {
        for (size_t index = 0; index < polygon.size(); ++index) {
            if (segmentsIntersect(cubic.p0, cubic.p3, polygon[index], polygon[(index + 1) % polygon.size()]))
                return true;
        }
        return false;
    }
    Cubic left;
    Cubic right;
    splitCubic(cubic, left, right);
    return crossesPolygon(left, polygon, depth + 1) || crossesPolygon(right, polygon, depth + 1);
}

} // namespace

bool RectD::isFinite() const
{
    return std::isfinite(x) && std::isfinite(y) && std::isfinite(w) && std::isfinite(h);
}

bool RectD::contains(PointD point) const
{
    return point.x >= x && point.y >= y && point.x <= x + w && point.y <= y + h;
}

bool RectD::intersects(const RectD &other) const
{
    return x < other.x + other.w && x + w > other.x && y < other.y + other.h && y + h > other.y;
}

bool RectD::containsRect(const RectD &inner) const
{
    return inner.x >= x - 1e-9 && inner.y >= y - 1e-9 && inner.x + inner.w <= x + w + 1e-9
        && inner.y + inner.h <= y + h + 1e-9;
}

RectD RectD::united(const RectD &other) const
{
    if (!(w > 0 || h > 0))
        return other;
    if (!(other.w > 0 || other.h > 0))
        return *this;
    const double left = std::min(x, other.x);
    const double top = std::min(y, other.y);
    const double right = std::max(x + w, other.x + other.w);
    const double bottom = std::max(y + h, other.y + other.h);
    return {left, top, right - left, bottom - top};
}

RectD RectD::expanded(double pad) const
{
    return {x - pad, y - pad, w + pad * 2, h + pad * 2};
}

Cubic Cubic::translated(double tx, double ty) const
{
    const PointD offset {tx, ty};
    return {p0 + offset, p1 + offset, p2 + offset, p3 + offset};
}

RectD Cubic::controlBounds() const
{
    const double left = std::min({p0.x, p1.x, p2.x, p3.x});
    const double top = std::min({p0.y, p1.y, p2.y, p3.y});
    const double right = std::max({p0.x, p1.x, p2.x, p3.x});
    const double bottom = std::max({p0.y, p1.y, p2.y, p3.y});
    return {left, top, right - left, bottom - top};
}

double distance(PointD a, PointD b)
{
    return std::hypot(a.x - b.x, a.y - b.y);
}

bool nearlyEqual(double a, double b, double epsilon)
{
    return std::abs(a - b) <= epsilon;
}

RectD boundsOfSegment(PointD a, PointD b)
{
    const double left = std::min(a.x, b.x);
    const double top = std::min(a.y, b.y);
    return {left, top, std::abs(a.x - b.x), std::abs(a.y - b.y)};
}

double pointSegmentDistance(PointD point, PointD a, PointD b)
{
    const PointD delta = b - a;
    const double lengthSquared = delta.x * delta.x + delta.y * delta.y;
    if (lengthSquared <= 1e-24)
        return distance(point, a);
    const double t = std::clamp(((point.x - a.x) * delta.x + (point.y - a.y) * delta.y) / lengthSquared, 0.0, 1.0);
    return distance(point, a + delta * t);
}

double segmentDistance(PointD a, PointD b, PointD c, PointD d)
{
    if (segmentsIntersect(a, b, c, d))
        return 0;
    return std::min({pointSegmentDistance(a, c, d), pointSegmentDistance(b, c, d),
                     pointSegmentDistance(c, a, b), pointSegmentDistance(d, a, b)});
}

bool segmentsIntersect(PointD a, PointD b, PointD c, PointD d)
{
    const int s1 = sideOf(a, b, c);
    const int s2 = sideOf(a, b, d);
    const int s3 = sideOf(c, d, a);
    const int s4 = sideOf(c, d, b);
    if (s1 && s2 && s3 && s4 && s1 != s2 && s3 != s4)
        return true;
    if (s1 == 0 && pointOnSegment(a, b, c))
        return true;
    if (s2 == 0 && pointOnSegment(a, b, d))
        return true;
    if (s3 == 0 && pointOnSegment(c, d, a))
        return true;
    if (s4 == 0 && pointOnSegment(c, d, b))
        return true;
    return false;
}

bool pointInPolygon(PointD point, const std::vector<PointD> &polygon)
{
    if (polygon.size() < 3)
        return false;
    bool inside = false;
    for (size_t index = 0, previous = polygon.size() - 1; index < polygon.size(); previous = index++) {
        const PointD &a = polygon[previous];
        const PointD &b = polygon[index];
        const bool crosses = (a.y > point.y) != (b.y > point.y)
            && point.x < (b.x - a.x) * (point.y - a.y) / (b.y - a.y) + a.x;
        if (crosses)
            inside = !inside;
    }
    return inside;
}

double distanceToPolygonBoundary(PointD point, const std::vector<PointD> &polygon)
{
    if (polygon.size() < 2)
        return std::numeric_limits<double>::infinity();
    double best = std::numeric_limits<double>::infinity();
    for (size_t index = 0; index < polygon.size(); ++index)
        best = std::min(best, pointSegmentDistance(point, polygon[index], polygon[(index + 1) % polygon.size()]));
    return best;
}

bool insideByMargin(PointD point, const std::vector<PointD> &polygon, double margin)
{
    if (!pointInPolygon(point, polygon))
        return false;
    return margin <= 0 || distanceToPolygonBoundary(point, polygon) > margin;
}

RectD polygonBounds(const std::vector<PointD> &polygon)
{
    if (polygon.empty())
        return {};
    double left = polygon.front().x;
    double top = polygon.front().y;
    double right = left;
    double bottom = top;
    for (PointD point : polygon) {
        left = std::min(left, point.x);
        top = std::min(top, point.y);
        right = std::max(right, point.x);
        bottom = std::max(bottom, point.y);
    }
    return {left, top, right - left, bottom - top};
}

bool cubicHitsSegment(const Cubic &cubic, PointD a, PointD b, double radius)
{
    return hitsSegment(cubic, a, b, radius, 0);
}

bool cubicCrossesPolygon(const Cubic &cubic, const std::vector<PointD> &polygon)
{
    if (polygon.size() < 2)
        return false;
    return crossesPolygon(cubic, polygon, 0);
}

bool cubicIsDot(const Cubic &cubic)
{
    return distance(cubic.p0, cubic.p1) < 1e-9 && distance(cubic.p0, cubic.p2) < 1e-9
        && distance(cubic.p0, cubic.p3) < 1e-9;
}

std::vector<Cubic> cubicsFromSamples(const std::vector<PointD> &samples)
{
    std::vector<PointD> points;
    points.reserve(samples.size());
    for (PointD sample : samples) {
        if (!finitePoint(sample))
            continue;
        if (!points.empty() && distance(points.back(), sample) < 1e-6)
            continue;
        points.push_back(sample);
    }
    if (points.empty())
        return {};
    if (points.size() == 1)
        return {{points[0], points[0], points[0], points[0]}};

    std::vector<Cubic> cubics;
    cubics.reserve(points.size() - 1);
    for (size_t index = 0; index + 1 < points.size(); ++index) {
        const PointD p0 = index == 0 ? points[0] : points[index - 1];
        const PointD p1 = points[index];
        const PointD p2 = points[index + 1];
        const PointD p3 = index + 2 < points.size() ? points[index + 2] : p2;
        cubics.push_back(catmullSegment(p0, p1, p2, p3));
    }
    return cubics;
}

} // namespace drawpad

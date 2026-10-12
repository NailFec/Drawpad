// SPDX-License-Identifier: Apache-2.0
// Copyright NailFec 2026 <nailfec17@gmail.com>

#pragma once

#include "geometry.h"

#include <QColor>

#include <cstdint>
#include <memory>
#include <vector>

namespace drawpad {

struct Stroke {
    std::uint64_t id = 0;
    QColor color = Qt::black;
    double width = 1;
    double tx = 0;
    double ty = 0;
    std::vector<PointD> samples;
    std::vector<Cubic> cubics;
    bool frozen = false;

    RectD worldBounds() const;
};

class StrokeDocument {
public:
    StrokeDocument();
    ~StrokeDocument();

    StrokeDocument(const StrokeDocument &) = delete;
    StrokeDocument &operator=(const StrokeDocument &) = delete;

    std::uint64_t beginStroke(const QColor &color, double width);
    void addSample(std::uint64_t id, PointD sample);
    void commitStroke(std::uint64_t id);
    void cancelStroke(std::uint64_t id);

    std::uint64_t insertFrozen(std::uint64_t id, const QColor &color, double width, std::vector<Cubic> cubics,
                               double tx, double ty);

    void eraseAlong(PointD from, PointD to, double eraserWidth);
    std::vector<std::uint64_t> selectInside(const std::vector<PointD> &lasso) const;
    void translate(const std::vector<std::uint64_t> &ids, double dx, double dy);

    bool contains(std::uint64_t id) const;
    const Stroke *stroke(std::uint64_t id) const;
    RectD boundsOf(const std::vector<std::uint64_t> &ids) const;
    std::vector<std::uint64_t> query(const RectD &area) const;
    std::vector<Stroke> frozenStrokes() const;

    void swap(StrokeDocument &other);

private:
    struct Data;
    std::unique_ptr<Data> m_data;
};

} // namespace drawpad

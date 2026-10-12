// SPDX-License-Identifier: Apache-2.0
// Copyright NailFec 2026 <nailfec17@gmail.com>

#include "stroke_document.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <utility>

namespace drawpad {
namespace {

constexpr int kCapacity = 8;
constexpr int kMaxDepth = 24;
constexpr double kMaxStrokeWidth = 64;

RectD quadrantRect(const RectD &parent, int index)
{
    const double halfWidth = parent.w * 0.5;
    const double halfHeight = parent.h * 0.5;
    return {parent.x + (index % 2) * halfWidth, parent.y + (index / 2) * halfHeight, halfWidth, halfHeight};
}

class Quadtree {
public:
    void clear() { m_root.reset(); }

    void insert(std::uint64_t id, const RectD &bounds)
    {
        if (!bounds.isFinite())
            return;
        ensure(bounds);
        insert(*m_root, {id, bounds}, 0);
    }

    void remove(std::uint64_t id)
    {
        if (m_root)
            remove(*m_root, id);
    }

    void update(std::uint64_t id, const RectD &bounds)
    {
        remove(id);
        insert(id, bounds);
    }

    std::vector<std::uint64_t> query(const RectD &area) const
    {
        std::vector<std::uint64_t> ids;
        if (m_root)
            query(*m_root, area, ids);
        return ids;
    }

private:
    struct Item {
        std::uint64_t id = 0;
        RectD bounds;
    };

    struct Node {
        RectD bounds;
        std::vector<Item> items;
        std::unique_ptr<Node> child[4];

        bool hasChildren() const { return child[0] || child[1] || child[2] || child[3]; }
    };

    void ensure(const RectD &bounds)
    {
        RectD needed = bounds.expanded(1);
        if (!(needed.w > 0)) {
            needed.x -= 0.5;
            needed.w = 1;
        }
        if (!(needed.h > 0)) {
            needed.y -= 0.5;
            needed.h = 1;
        }
        if (!m_root) {
            m_root = std::make_unique<Node>();
            m_root->bounds = needed;
            return;
        }
        int guard = 0;
        while (m_root && !m_root->bounds.containsRect(needed) && guard < 64) {
            const RectD current = m_root->bounds;
            const bool shiftX = needed.x < current.x;
            const bool shiftY = needed.y < current.y;
            auto parent = std::make_unique<Node>();
            parent->bounds = {shiftX ? current.x - current.w : current.x, shiftY ? current.y - current.h : current.y,
                              current.w * 2, current.h * 2};
            const double centerX = current.x + current.w * 0.5;
            const double centerY = current.y + current.h * 0.5;
            const double midX = parent->bounds.x + parent->bounds.w * 0.5;
            const double midY = parent->bounds.y + parent->bounds.h * 0.5;
            const int index = (centerY >= midY ? 2 : 0) + (centerX >= midX ? 1 : 0);
            parent->child[index] = std::move(m_root);
            m_root = std::move(parent);
            ++guard;
        }
    }

    static bool fitsQuadrant(const RectD &parent, const RectD &item, int &index)
    {
        for (int candidate = 0; candidate < 4; ++candidate) {
            if (quadrantRect(parent, candidate).containsRect(item)) {
                index = candidate;
                return true;
            }
        }
        return false;
    }

    void insert(Node &node, const Item &item, int depth)
    {
        if (node.hasChildren()) {
            int index = 0;
            if (depth < kMaxDepth && fitsQuadrant(node.bounds, item.bounds, index)) {
                if (!node.child[index]) {
                    node.child[index] = std::make_unique<Node>();
                    node.child[index]->bounds = quadrantRect(node.bounds, index);
                }
                insert(*node.child[index], item, depth + 1);
            } else {
                node.items.push_back(item);
            }
            return;
        }

        node.items.push_back(item);
        if (node.items.size() <= kCapacity || depth >= kMaxDepth)
            return;

        const std::vector<Item> items = std::move(node.items);
        node.items.clear();
        for (int index = 0; index < 4; ++index) {
            node.child[index] = std::make_unique<Node>();
            node.child[index]->bounds = quadrantRect(node.bounds, index);
        }
        for (const Item &existing : items)
            insert(node, existing, depth);
    }

    bool remove(Node &node, std::uint64_t id)
    {
        const auto found = std::find_if(node.items.begin(), node.items.end(),
                                        [id](const Item &item) { return item.id == id; });
        if (found != node.items.end()) {
            node.items.erase(found);
            return true;
        }
        for (int index = 0; index < 4; ++index) {
            if (node.child[index] && remove(*node.child[index], id))
                return true;
        }
        return false;
    }

    void query(const Node &node, const RectD &area, std::vector<std::uint64_t> &ids) const
    {
        if (!node.bounds.intersects(area))
            return;
        for (const Item &item : node.items) {
            if (item.bounds.intersects(area))
                ids.push_back(item.id);
        }
        for (int index = 0; index < 4; ++index) {
            if (node.child[index])
                query(*node.child[index], area, ids);
        }
    }

    std::unique_ptr<Node> m_root;
};

bool strokeInside(const Stroke &stroke, const std::vector<PointD> &lasso)
{
    if (!stroke.frozen || stroke.cubics.empty() || lasso.size() < 3)
        return false;
    const double margin = stroke.width * 0.5;
    for (const Cubic &cubic : stroke.cubics) {
        const Cubic world = cubic.translated(stroke.tx, stroke.ty);
        const PointD controls[4] = {world.p0, world.p1, world.p2, world.p3};
        for (PointD control : controls) {
            if (!insideByMargin(control, lasso, margin))
                return false;
        }
        if (cubicCrossesPolygon(world, lasso))
            return false;
    }
    return true;
}

} // namespace

RectD Stroke::worldBounds() const
{
    bool any = false;
    double left = 0;
    double top = 0;
    double right = 0;
    double bottom = 0;
    const auto accumulate = [&](PointD point) {
        if (!any) {
            left = right = point.x;
            top = bottom = point.y;
            any = true;
            return;
        }
        left = std::min(left, point.x);
        top = std::min(top, point.y);
        right = std::max(right, point.x);
        bottom = std::max(bottom, point.y);
    };
    for (const Cubic &cubic : cubics) {
        accumulate(cubic.p0);
        accumulate(cubic.p1);
        accumulate(cubic.p2);
        accumulate(cubic.p3);
    }
    if (!frozen) {
        for (PointD sample : samples)
            accumulate(sample);
    }
    if (!any)
        return {};
    const double pad = std::max(width * 0.5, 0.0);
    return {left + tx - pad, top + ty - pad, (right - left) + pad * 2, (bottom - top) + pad * 2};
}

struct StrokeDocument::Data {
    std::unordered_map<std::uint64_t, Stroke> strokes;
    Quadtree index;
    std::uint64_t nextId = 1;
};

StrokeDocument::StrokeDocument()
    : m_data(std::make_unique<Data>())
{
}

StrokeDocument::~StrokeDocument() = default;

std::uint64_t StrokeDocument::beginStroke(const QColor &color, double width)
{
    Stroke stroke;
    stroke.id = m_data->nextId++;
    stroke.color = color;
    stroke.width = width;
    const std::uint64_t id = stroke.id;
    m_data->strokes.emplace(id, std::move(stroke));
    return id;
}

void StrokeDocument::addSample(std::uint64_t id, PointD sample)
{
    const auto found = m_data->strokes.find(id);
    if (found == m_data->strokes.end() || found->second.frozen)
        return;
    if (!std::isfinite(sample.x) || !std::isfinite(sample.y))
        return;
    if (!found->second.samples.empty() && distance(found->second.samples.back(), sample) < 1e-6)
        return;
    found->second.samples.push_back(sample);
}

void StrokeDocument::commitStroke(std::uint64_t id)
{
    const auto found = m_data->strokes.find(id);
    if (found == m_data->strokes.end() || found->second.frozen)
        return;
    found->second.cubics = cubicsFromSamples(found->second.samples);
    found->second.samples.clear();
    found->second.frozen = true;
    if (found->second.cubics.empty()) {
        m_data->strokes.erase(found);
        return;
    }
    m_data->index.insert(id, found->second.worldBounds());
}

void StrokeDocument::cancelStroke(std::uint64_t id)
{
    const auto found = m_data->strokes.find(id);
    if (found == m_data->strokes.end())
        return;
    if (found->second.frozen)
        m_data->index.remove(id);
    m_data->strokes.erase(found);
}

std::uint64_t StrokeDocument::insertFrozen(std::uint64_t id, const QColor &color, double width,
                                           std::vector<Cubic> cubics, double tx, double ty)
{
    if (cubics.empty())
        return 0;
    if (id == 0)
        id = m_data->nextId;
    if (m_data->strokes.contains(id))
        return 0;
    m_data->nextId = std::max(m_data->nextId, id + 1);
    Stroke stroke;
    stroke.id = id;
    stroke.color = color;
    stroke.width = width;
    stroke.cubics = std::move(cubics);
    stroke.tx = tx;
    stroke.ty = ty;
    stroke.frozen = true;
    m_data->index.insert(id, stroke.worldBounds());
    m_data->strokes.emplace(id, std::move(stroke));
    return id;
}

void StrokeDocument::eraseAlong(PointD from, PointD to, double eraserWidth)
{
    const RectD area = boundsOfSegment(from, to).expanded(eraserWidth * 0.5 + kMaxStrokeWidth);
    const std::vector<std::uint64_t> ids = m_data->index.query(area);
    for (std::uint64_t id : ids) {
        const auto found = m_data->strokes.find(id);
        if (found == m_data->strokes.end() || !found->second.frozen)
            continue;
        Stroke &stroke = found->second;
        const double radius = eraserWidth * 0.5 + stroke.width * 0.5;
        std::vector<Cubic> kept;
        kept.reserve(stroke.cubics.size());
        bool changed = false;
        for (const Cubic &cubic : stroke.cubics) {
            if (cubicHitsSegment(cubic.translated(stroke.tx, stroke.ty), from, to, radius))
                changed = true;
            else
                kept.push_back(cubic);
        }
        if (!changed)
            continue;
        if (kept.empty()) {
            m_data->index.remove(id);
            m_data->strokes.erase(found);
            continue;
        }
        stroke.cubics = std::move(kept);
        m_data->index.update(id, stroke.worldBounds());
    }
}

std::vector<std::uint64_t> StrokeDocument::selectInside(const std::vector<PointD> &lasso) const
{
    std::vector<std::uint64_t> selected;
    if (lasso.size() < 3)
        return selected;
    const std::vector<std::uint64_t> ids = m_data->index.query(polygonBounds(lasso));
    for (std::uint64_t id : ids) {
        const auto found = m_data->strokes.find(id);
        if (found != m_data->strokes.end() && strokeInside(found->second, lasso))
            selected.push_back(id);
    }
    return selected;
}

void StrokeDocument::translate(const std::vector<std::uint64_t> &ids, double dx, double dy)
{
    if (dx == 0 && dy == 0)
        return;
    for (std::uint64_t id : ids) {
        const auto found = m_data->strokes.find(id);
        if (found == m_data->strokes.end() || !found->second.frozen)
            continue;
        found->second.tx += dx;
        found->second.ty += dy;
        m_data->index.update(id, found->second.worldBounds());
    }
}

bool StrokeDocument::contains(std::uint64_t id) const
{
    return m_data->strokes.contains(id);
}

const Stroke *StrokeDocument::stroke(std::uint64_t id) const
{
    const auto found = m_data->strokes.find(id);
    if (found == m_data->strokes.end())
        return nullptr;
    return &found->second;
}

RectD StrokeDocument::boundsOf(const std::vector<std::uint64_t> &ids) const
{
    RectD bounds;
    bool any = false;
    for (std::uint64_t id : ids) {
        const Stroke *found = stroke(id);
        if (!found)
            continue;
        const RectD next = found->worldBounds();
        bounds = any ? bounds.united(next) : next;
        any = true;
    }
    return bounds;
}

std::vector<std::uint64_t> StrokeDocument::query(const RectD &area) const
{
    return m_data->index.query(area);
}

std::vector<Stroke> StrokeDocument::frozenStrokes() const
{
    std::vector<Stroke> strokes;
    strokes.reserve(m_data->strokes.size());
    for (const auto &entry : m_data->strokes) {
        if (entry.second.frozen)
            strokes.push_back(entry.second);
    }
    std::sort(strokes.begin(), strokes.end(),
              [](const Stroke &left, const Stroke &right) { return left.id < right.id; });
    return strokes;
}

void StrokeDocument::swap(StrokeDocument &other)
{
    m_data.swap(other.m_data);
}

} // namespace drawpad

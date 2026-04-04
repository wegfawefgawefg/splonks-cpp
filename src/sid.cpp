#include "sid.hpp"

namespace splonks {

namespace {

bool Intersects(const AABB& left, const AABB& right) {
    if (left.br.x < right.tl.x) {
        return false;
    }
    if (left.tl.x > right.br.x) {
        return false;
    }
    if (left.br.y < right.tl.y) {
        return false;
    }
    if (left.tl.y > right.br.y) {
        return false;
    }
    return true;
}

} // namespace

AABB SIDNode::GetAABB() const {
    return AABB::New(pos, pos + size);
}

SID SID::New() {
    SID sid;
    return sid;
}

void SID::Insert(const VID& vid, const Vec2& pos, const Vec2& size) {
    const Vec2 adjusted_size = size - Vec2::New(1.0F, 1.0F);

    SIDNode node;
    node.vid = vid;
    node.pos = pos;
    node.size = adjusted_size;
    nodes_.push_back(node);
}

std::vector<VID> SID::Query(const Vec2& top_left, const Vec2& bottom_right) const {
    const AABB query_box = AABB::New(top_left, bottom_right);

    std::vector<VID> result;
    for (const SIDNode& node : nodes_) {
        if (Intersects(node.GetAABB(), query_box)) {
            result.push_back(node.vid);
        }
    }
    return result;
}

std::vector<VID> SID::QueryExclude(const Vec2& top_left, const Vec2& bottom_right,
                                   const VID& exclude_vid) const {
    const AABB query_box = AABB::New(top_left, bottom_right);

    std::vector<VID> result;
    for (const SIDNode& node : nodes_) {
        if (node.vid.id == exclude_vid.id) {
            continue;
        }
        if (Intersects(node.GetAABB(), query_box)) {
            result.push_back(node.vid);
        }
    }
    return result;
}

std::vector<VIDAABB> SID::QueryForVIDAABBsExclude(const Vec2& top_left, const Vec2& bottom_right,
                                                  const VID& exclude_vid) const {
    const AABB query_box = AABB::New(top_left, bottom_right);

    std::vector<VIDAABB> result;
    for (const SIDNode& node : nodes_) {
        if (node.vid.id == exclude_vid.id) {
            continue;
        }
        if (Intersects(node.GetAABB(), query_box)) {
            VIDAABB vidaabb;
            vidaabb.vid = node.vid;
            vidaabb.aabb = node.GetAABB();
            result.push_back(vidaabb);
        }
    }
    return result;
}

} // namespace splonks

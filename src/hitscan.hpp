#pragma once

#include "entity.hpp"
#include "graphics.hpp"
#include "state.hpp"

namespace splonks {

enum class HitscanHitType {
    None,
    StageBounds,
    Tile,
    Entity,
};

struct HitscanHit {
    HitscanHitType type = HitscanHitType::None;
    IVec2 point = IVec2::New(0, 0);
    std::optional<VID> entity_vid = std::nullopt;
};

HitscanHit TraceHitscan(
    const Entity& source_entity,
    const Vec2& start_pos,
    int direction,
    int max_distance,
    State& state,
    const Graphics& graphics,
    std::optional<VID> owner_vid
);

} // namespace splonks

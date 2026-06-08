#pragma once

#include "ent.hpp"
#include "graphics.hpp"
#include "state.hpp"

namespace splonks {

enum class HitscanHitType {
    None,
    StageBounds,
    Tile,
    Ent,
};

struct HitscanHit {
    HitscanHitType type = HitscanHitType::None;
    IVec2 point = IVec2::New(0, 0);
    std::optional<VID> ent_vid = std::nullopt;
};

HitscanHit TraceHitscan(
    const Ent& source_ent,
    sim::Vec2 start_pos,
    int direction,
    int max_distance,
    State& state,
    const Graphics& graphics,
    std::optional<VID> owner_vid
);

} // namespace splonks

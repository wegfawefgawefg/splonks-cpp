#pragma once

#include "graphics.hpp"
#include "state.hpp"

namespace splonks::ents::common {

bool IsSolidTileAtWorldPos(const State& state, const IVec2& world_pos);
bool HasWallAheadForGroundWalker(
    const Ent& ent,
    const State& state,
    const Graphics& graphics,
    int direction
);
bool HasGroundAheadForGroundWalker(
    const Ent& ent,
    const State& state,
    const Graphics& graphics,
    int direction
);

} // namespace splonks::ents::common

#pragma once

#include "graphics.hpp"
#include "state.hpp"

namespace splonks::entities::common {

bool IsSolidTileAtWorldPos(const State& state, const IVec2& world_pos);
bool HasWallAheadForGroundWalker(
    const Entity& entity,
    const State& state,
    const Graphics& graphics,
    int direction
);
bool HasGroundAheadForGroundWalker(
    const Entity& entity,
    const State& state,
    const Graphics& graphics,
    int direction
);

} // namespace splonks::entities::common

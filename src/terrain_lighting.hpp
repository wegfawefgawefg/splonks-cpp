#pragma once

#include "math_types.hpp"
#include "stage_lighting.hpp"

namespace splonks {

struct State;
void InvalidateTerrainLightingCache(State& state);
void RebuildTerrainLightingCache(State& state);
void EnsureTerrainLightingCache(State& state);
void UpdateTerrainLightingCacheForTileChange(State& state, const IVec2& tile_pos);
TerrainLightingTile GetTerrainLightingTileForRender(
    const State& state,
    int tile_x,
    int tile_y
);
BackwallLightingTile GetBackwallLightingTileForRender(
    const State& state,
    int tile_x,
    int tile_y
);

} // namespace splonks

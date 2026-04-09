#pragma once

#include "math_types.hpp"

#include <vector>

namespace splonks {

struct Graphics;
struct State;

struct TerrainLightingTile {
    float brightness = 1.0F;
    bool open_top = false;
    bool open_bottom = false;
    bool open_left = false;
    bool open_right = false;
    bool ao_top_left = false;
    bool ao_top_right = false;
    bool ao_bottom_left = false;
    bool ao_bottom_right = false;
};

struct TerrainLightingCache {
    std::vector<std::vector<TerrainLightingTile>> tiles;
    bool valid = false;

    static TerrainLightingCache New();
};

struct BackwallLightingTile {
    float brightness = 1.0F;
};

struct BackwallLightingCache {
    std::vector<std::vector<BackwallLightingTile>> tiles;
    bool valid = false;

    static BackwallLightingCache New();
};

void InvalidateTerrainLightingCache(Graphics& graphics);
void RebuildTerrainLightingCache(const State& state, Graphics& graphics);
void EnsureTerrainLightingCache(const State& state, Graphics& graphics);
void UpdateTerrainLightingCacheForTileChange(
    const State& state,
    Graphics& graphics,
    const IVec2& tile_pos
);
TerrainLightingTile GetTerrainLightingTileForRender(
    const State& state,
    const Graphics& graphics,
    int tile_x,
    int tile_y
);
BackwallLightingTile GetBackwallLightingTileForRender(
    const State& state,
    const Graphics& graphics,
    int tile_x,
    int tile_y
);

} // namespace splonks

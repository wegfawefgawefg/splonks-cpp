#pragma once

#include "math_types.hpp"

#include <vector>

namespace splonks {

struct ForegroundTileTopology {
    bool open_top = false;
    bool open_bottom = false;
    bool open_left = false;
    bool open_right = false;
    bool ao_top_left = false;
    bool ao_top_right = false;
    bool ao_bottom_left = false;
    bool ao_bottom_right = false;
};

struct ForegroundTopologyCache {
    std::vector<std::vector<ForegroundTileTopology>> tiles;
    bool valid = false;

    static ForegroundTopologyCache New();
};

struct ForegroundBrightnessCache {
    std::vector<std::vector<float>> tiles;
    bool valid = false;

    static ForegroundBrightnessCache New();
};

struct BackwallBrightnessCache {
    std::vector<std::vector<float>> tiles;
    bool valid = false;

    static BackwallBrightnessCache New();
};

struct StageLighting {
    ForegroundTopologyCache foreground_topology;
    ForegroundBrightnessCache foreground_brightness;
    BackwallBrightnessCache backwall_brightness;

    static StageLighting New();
};

struct State;
void InvalidateStageLighting(State& state);
void RebuildStageLighting(State& state);
void EnsureStageLighting(State& state);
void UpdateStageLightingForTileChange(State& state, const IVec2& tile_pos);
ForegroundTileTopology GetForegroundTileTopologyForRender(
    const State& state,
    int tile_x,
    int tile_y
);
float GetForegroundBrightnessForRender(const State& state, int tile_x, int tile_y);
float GetBackwallBrightnessForRender(const State& state, int tile_x, int tile_y);

} // namespace splonks

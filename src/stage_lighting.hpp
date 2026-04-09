#pragma once

#include <vector>

namespace splonks {

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

struct StageLighting {
    TerrainLightingCache terrain;
    BackwallLightingCache backwall;

    static StageLighting New();
};

} // namespace splonks

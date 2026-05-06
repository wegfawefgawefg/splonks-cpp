#pragma once

#include "audio.hpp"
#include "math_types.hpp"
#include "state.hpp"

#include <optional>
#include <vector>

namespace splonks {

void BreakStageTilesInRectWc(
    const AABB& area,
    State& state,
    Audio& audio,
    std::optional<AudioAssetId> override_break_sound = std::nullopt,
    bool suppress_tile_break_sound = false,
    bool suppress_network_event = false,
    bool suppress_drop_spawns = false
);

void BreakStageTilesAtCoords(
    const std::vector<IVec2>& tile_positions,
    State& state,
    Audio& audio,
    std::optional<AudioAssetId> override_break_sound = std::nullopt,
    bool suppress_tile_break_sound = false,
    bool suppress_network_event = false,
    bool suppress_drop_spawns = false
);

} // namespace splonks

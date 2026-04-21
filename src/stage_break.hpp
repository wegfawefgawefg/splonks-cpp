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
    bool suppress_tile_break_sound = false
);

void BreakStageTilesAtCoords(
    const std::vector<IVec2>& tile_positions,
    State& state,
    Audio& audio,
    std::optional<AudioAssetId> override_break_sound = std::nullopt,
    bool suppress_tile_break_sound = false
);

} // namespace splonks

#pragma once

#include "audio.hpp"
#include "math_types.hpp"
#include "state.hpp"

#include <optional>

namespace splonks {

void BreakStageTilesInRectWc(
    const AABB& area,
    State& state,
    Audio& audio,
    std::optional<AudioAssetId> override_break_sound = std::nullopt,
    bool suppress_tile_break_sound = false
);

} // namespace splonks

#pragma once

#include "audio.hpp"
#include "math_types.hpp"
#include "state.hpp"

namespace splonks {

void BreakStageTilesInRectWc(const AABB& area, State& state, Audio& audio);

} // namespace splonks

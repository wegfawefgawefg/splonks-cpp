#pragma once

#include "stage.hpp"

namespace splonks {

struct Audio;
struct State;

void DispatchStageTileTriggerEvent(StageTileTriggerEvent event, const IVec2& tile_pos, State& state, Audio& audio);
void DispatchStageTileDestroyed(const IVec2& tile_pos, State& state, Audio& audio);

} // namespace splonks

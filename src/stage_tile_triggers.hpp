#pragma once

#include "stage.hpp"

namespace splonks {

struct Audio;
struct State;

void RunStageTileTriggers(StageTileTriggerKind kind, const IVec2& tile_pos, State& state, Audio& audio);
void RunStageTileDestroyedTriggers(const IVec2& tile_pos, State& state, Audio& audio);

} // namespace splonks

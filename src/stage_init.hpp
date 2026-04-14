#pragma once

#include "state.hpp"

namespace splonks {

void InitStage(State& state, bool preserve_player_state = false);
void InitDebugLevel(State& state);

} // namespace splonks

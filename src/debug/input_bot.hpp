#pragma once

#include "inputs.hpp"
#include "state.hpp"

namespace splonks::debug {

PlayingInputs MakeDebugBotInputs(DebugLocalPlayerBot& bot);
void ApplyDebugPrimaryPlayerBotInput(State& state);

} // namespace splonks::debug

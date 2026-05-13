#pragma once

#include "audio.hpp"
#include "graphics.hpp"
#include "state.hpp"

namespace splonks {

void StepEnts(State& state, Audio& audio, Graphics& graphics, float dt);

} // namespace splonks

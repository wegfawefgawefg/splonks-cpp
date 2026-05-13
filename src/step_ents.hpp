#pragma once

#include "audio.hpp"
#include "graphics.hpp"
#include "state.hpp"

namespace splonks {

void StepEntities(State& state, Audio& audio, Graphics& graphics, float dt);

} // namespace splonks

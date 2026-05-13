#pragma once

#include "ent/spec.hpp"

namespace splonks::ents::entrance {

extern const EntSpec kEntranceSpec;

void StepEntLogicAsEntrance(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::ents::entrance

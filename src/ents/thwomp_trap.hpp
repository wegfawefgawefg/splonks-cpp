#pragma once

#include "ent/spec.hpp"

namespace splonks::ents::thwomp_trap {

extern const EntSpec kThwompTrapSpec;

void StepEntLogicAsThwompTrap(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

void StepEntPhysicsAsThwompTrap(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::ents::thwomp_trap

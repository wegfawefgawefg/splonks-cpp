#pragma once

#include "ent/spec.hpp"

namespace splonks::ents::door {

extern const EntSpec kDoorSpec;

void StepEntLogicAsDoor(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

void StepEntPhysicsAsDoor(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::ents::door

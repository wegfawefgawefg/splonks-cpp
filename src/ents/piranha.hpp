#pragma once

#include "ent/spec.hpp"

namespace splonks::ents::piranha {

extern const EntSpec kPiranhaSpec;

void StepEntLogicAsPiranha(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

void StepEntPhysicsAsPiranha(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::ents::piranha

#pragma once

#include "ent/spec.hpp"

namespace splonks::ents::trap_block {

extern const EntSpec kTrapBlockSpec;

void MakeTrapBlockOneShot(Ent& block);

void StepEntLogicAsTrapBlock(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

void StepEntPhysicsAsTrapBlock(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::ents::trap_block

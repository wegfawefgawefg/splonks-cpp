#pragma once

#include "ent/spec.hpp"

namespace splonks::ents::damsel {

void StepEntLogicAsDamsel(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

bool BuyDamsel(
    std::size_t ent_idx,
    std::size_t buyer_idx,
    State& state,
    Graphics& graphics,
    Audio& audio
);

extern const EntSpec kDamselSpec;

} // namespace splonks::ents::damsel

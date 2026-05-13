#pragma once

#include "ent/spec.hpp"

namespace splonks::ents::gold_idol {

void StepEntLogicAsGoldIdol(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

extern const EntSpec kGoldIdolSpec;

} // namespace splonks::ents::gold_idol

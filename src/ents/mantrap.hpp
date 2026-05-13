#pragma once

#include "ent/spec.hpp"

namespace splonks::ents::mantrap {

extern const EntSpec kMantrapSpec;

void StepEntLogicAsMantrap(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::ents::mantrap

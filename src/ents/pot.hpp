#pragma once

#include "ent/spec.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::ents::common {
struct ContactContext;
}

namespace splonks::ents::pot {

extern const EntSpec kPotSpec;

void StepEntLogicAsPot(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);
void OnDeathAsPot(std::size_t ent_idx, State& state, Audio& audio);
bool TryApplyPotImpact(
    std::size_t ent_idx,
    const common::ContactContext& context,
    State& state
);

} // namespace splonks::ents::pot

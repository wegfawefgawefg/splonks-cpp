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

namespace splonks::ents::box {

extern const EntSpec kBoxSpec;

void StepEntLogicAsBox(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);
void OnDeathAsBox(std::size_t ent_idx, State& state, Audio& audio);
bool TryApplyBoxImpact(
    std::size_t ent_idx,
    const common::ContactContext& context,
    State& state
);

} // namespace splonks::ents::box

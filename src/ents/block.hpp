#pragma once

#include "ent/spec.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;
namespace ents::common {
struct ContactContext;
}

}

namespace splonks::ents::block {

extern const EntSpec kBlockSpec;

void OnDeathAsBlock(std::size_t ent_idx, State& state, Audio& audio);
bool TryApplyBlockContactToEnt(
    std::size_t ent_idx,
    std::size_t other_ent_idx,
    const common::ContactContext& context,
    State& state,
    const Graphics& graphics,
    Audio& audio
);
void StepEntLogicAsBlock(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::ents::block

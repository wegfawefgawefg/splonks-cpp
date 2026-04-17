#pragma once

#include "entity/archetype.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;
namespace entities::common {
struct ContactContext;
}

}

namespace splonks::entities::block {

constexpr float kBlockPushAcc = 0.2F;

extern const EntityArchetype kBlockArchetype;

void OnDeathAsBlock(std::size_t entity_idx, State& state, Audio& audio);
bool TryApplyBlockContactToEntity(
    std::size_t entity_idx,
    std::size_t other_entity_idx,
    const common::ContactContext& context,
    State& state,
    const Graphics& graphics,
    Audio& audio
);
void StepEntityLogicAsBlock(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::block

#pragma once

#include "entity/archetype.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::common {
struct ContactContext;
}

namespace splonks::entities::box {

extern const EntityArchetype kBoxArchetype;

void StepEntityLogicAsBox(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);
void OnDeathAsBox(std::size_t entity_idx, State& state, Audio& audio);
bool TryApplyBoxImpact(
    std::size_t entity_idx,
    const common::ContactContext& context,
    State& state
);

} // namespace splonks::entities::box

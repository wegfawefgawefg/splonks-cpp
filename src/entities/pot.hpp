#pragma once

#include "entity_archetype.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::common {
struct ContactContext;
}

namespace splonks::entities::pot {

extern const EntityArchetype kPotArchetype;

void StepEntityLogicAsPot(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);
void OnDeathAsPot(std::size_t entity_idx, State& state, Audio& audio);
bool TryApplyPotImpact(
    std::size_t entity_idx,
    const common::ContactContext& context,
    State& state
);

} // namespace splonks::entities::pot

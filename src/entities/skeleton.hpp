#pragma once

#include "entity/archetype.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

} // namespace splonks

namespace splonks::entities::common {
struct ContactContext;
}

namespace splonks::entities::skeleton {

extern const EntityArchetype kSkullArchetype;
extern const EntityArchetype kSkeletonArchetype;

void StepEntityLogicAsSkeleton(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

void OnDeathAsSkull(std::size_t entity_idx, State& state, Audio& audio);
void OnDeathAsSkeleton(std::size_t entity_idx, State& state, Audio& audio);

bool TryApplySkullTileImpact(
    std::size_t entity_idx,
    const common::ContactContext& context,
    State& state
);

bool TryApplySkullEntityImpact(
    std::size_t entity_idx,
    std::size_t other_entity_idx,
    const common::ContactContext& context,
    State& state
);

} // namespace splonks::entities::skeleton

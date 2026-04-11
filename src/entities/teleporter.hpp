#pragma once

#include "entity_archetype.hpp"
#include "entities/common.hpp"

namespace splonks::entities::teleporter {

extern const EntityArchetype kTeleporterArchetype;

void StepEntityLogicAsTeleporter(std::size_t entity_idx, State& state, Audio& audio);
void StepEntityPhysicsAsTeleporter(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::teleporter

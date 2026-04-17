#pragma once

#include "entity/archetype.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::boulder {

extern const EntityArchetype kBoulderArchetype;
void SpawnBoulderBreakEffects(const Vec2& center, State& state);
void OnDeathAsBoulder(std::size_t entity_idx, State& state, Audio& audio);
void StepEntityLogicAsBoulder(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);
void StepEntityPhysicsAsBoulder(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::boulder

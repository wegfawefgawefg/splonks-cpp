#pragma once

#include "entity_archetype.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::scarab {

extern const EntityArchetype kScarabArchetype;

void StepEntityLogicAsScarab(std::size_t entity_idx, State& state, Audio& audio);
void StepEntityPhysicsAsScarab(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::scarab

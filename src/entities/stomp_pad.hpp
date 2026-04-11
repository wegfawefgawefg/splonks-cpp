#pragma once

#include "entity_archetype.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::stomp_pad {

extern const EntityArchetype kStompPadArchetype;

void StepEntityLogicAsStompPad(std::size_t entity_idx, State& state, Audio& audio);
void StepEntityPhysicsAsStompPad(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::stomp_pad

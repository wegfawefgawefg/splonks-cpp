#pragma once

#include "entity.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::stomp_pad {

void SetEntityStompPad(Entity& entity);
void StepEntityLogicAsStompPad(std::size_t entity_idx, State& state, Audio& audio);
void StepEntityPhysicsAsStompPad(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::stomp_pad

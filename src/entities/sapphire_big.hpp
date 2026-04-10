#pragma once

#include "entity.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::sapphire_big {

void SetEntitySapphireBig(Entity& entity);
void StepEntityLogicAsSapphireBig(std::size_t entity_idx, State& state, Audio& audio);
void StepEntityPhysicsAsSapphireBig(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::sapphire_big

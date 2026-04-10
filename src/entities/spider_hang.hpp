#pragma once

#include "entity.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::spider_hang {

void SetEntitySpiderHang(Entity& entity);
void SetEntityGiantSpiderHang(Entity& entity);
void StepEntityLogicAsSpiderHang(std::size_t entity_idx, State& state, Audio& audio);
void StepEntityPhysicsAsSpiderHang(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::spider_hang

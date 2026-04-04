#pragma once

#include "entity.hpp"

namespace splonks {

struct Audio;
struct State;

}

namespace splonks::entities::money {

constexpr IVec2 kSize = {8, 8};

void SetEntityMoney(Entity& entity, EntityType type_);
void StepEntityLogicAsMoney(std::size_t entity_idx, State& state, Audio& audio);
void StepEntityPhysicsAsMoney(std::size_t entity_idx, State& state, Audio& audio, float dt);
Vec2 GetSize(EntityType type_);

} // namespace splonks::entities::money

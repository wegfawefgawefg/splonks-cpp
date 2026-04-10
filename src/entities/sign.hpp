#pragma once

#include "entity.hpp"

namespace splonks {

struct Audio;
struct State;

}

namespace splonks::entities::sign {

void SetEntitySignGeneral(Entity& entity);
void SetEntitySignBomb(Entity& entity);
void SetEntitySignWeapon(Entity& entity);
void SetEntitySignRare(Entity& entity);
void SetEntitySignClothing(Entity& entity);
void SetEntitySignCraps(Entity& entity);
void SetEntitySignKissing(Entity& entity);
void StepEntityLogicAsSign(std::size_t entity_idx, State& state, Audio& audio);

} // namespace splonks::entities::sign

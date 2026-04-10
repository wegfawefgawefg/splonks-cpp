#pragma once

#include "entities/common.hpp"

namespace splonks::entities::gear_items {

void SetEntityCape(Entity& entity);
void SetEntityShotgun(Entity& entity);
void SetEntityTeleporter(Entity& entity);
void SetEntityGloves(Entity& entity);
void SetEntitySpectacles(Entity& entity);
void SetEntityWebCannon(Entity& entity);
void SetEntityPistol(Entity& entity);
void SetEntityMitt(Entity& entity);
void SetEntityPaste(Entity& entity);
void SetEntitySpringShoes(Entity& entity);
void SetEntitySpikeShoes(Entity& entity);
void SetEntityMachete(Entity& entity);
void SetEntityBombBox(Entity& entity);
void SetEntityBow(Entity& entity);
void SetEntityCompass(Entity& entity);
void SetEntityParachute(Entity& entity);
void SetEntityRopePile(Entity& entity);

void StepEntityLogicAsGearItem(std::size_t entity_idx, State& state, Audio& audio);
void StepEntityPhysicsAsGearItem(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::gear_items

#pragma once

#include "entity/archetype.hpp"

namespace splonks::entities::shopkeeper {

EntityDamageEffectResult OnDamageAsShopkeeper(
    std::size_t entity_idx,
    State& state,
    Audio& audio,
    DamageType damage_type,
    unsigned int amount,
    bool damage_applied
);

void StepEntityLogicAsShopkeeper(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

extern const EntityArchetype kShopkeeperArchetype;

} // namespace splonks::entities::shopkeeper

#pragma once

#include "entity/archetype.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::store_light {

constexpr int kStoreLightRadiusTiles = 5;

extern const EntityArchetype kStoreLightArchetype;

void AttachStoreLight(Entity& entity, State& state, int radius = kStoreLightRadiusTiles);
EntityDamageEffectResult OnDamageAsStoreLight(
    std::size_t entity_idx,
    State& state,
    Audio& audio,
    DamageType damage_type,
    unsigned int amount,
    bool damage_applied
);

} // namespace splonks::entities::store_light

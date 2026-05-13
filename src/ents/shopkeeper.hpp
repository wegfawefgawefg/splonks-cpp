#pragma once

#include "ent/spec.hpp"

namespace splonks::ents::shopkeeper {

EntDamageEffectResult OnDamageAsShopkeeper(
    std::size_t ent_idx,
    State& state,
    Audio& audio,
    DamageType damage_type,
    unsigned int amount,
    bool damage_applied
);

void StepEntLogicAsShopkeeper(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

extern const EntSpec kShopkeeperSpec;

} // namespace splonks::ents::shopkeeper

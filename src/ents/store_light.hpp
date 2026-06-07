#pragma once

#include "ent/spec.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::ents::store_light {

constexpr int kStoreLightRadiusTiles = 5;

extern const EntSpec kStoreLightSpec;

void AttachStoreLight(Ent& ent, State& state, int radius = kStoreLightRadiusTiles);
EntDamageEffectResult OnDamageAsStoreLight(
    std::size_t ent_idx,
    State& state,
    Audio& audio,
    DamageType damage_type,
    std::uint32_t amount,
    bool damage_applied
);

} // namespace splonks::ents::store_light

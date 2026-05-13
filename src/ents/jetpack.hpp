#pragma once

#include "entity/archetype.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

}

namespace splonks::entities::jetpack {

constexpr float kTravelSoundDistInterval = 8.0F;
constexpr float kFuel = 120.0F;

extern const EntityArchetype kJetPackArchetype;

void OnDeathAsJetpack(std::size_t entity_idx, State& state, Audio& audio);
EntityDamageEffectResult OnDamageAsJetpack(
    std::size_t entity_idx,
    State& state,
    Audio& audio,
    DamageType damage_type,
    unsigned int amount,
    bool damage_applied
);
void OnUseAsJetpack(std::size_t entity_idx, State& state, Graphics& graphics, Audio& audio);
void StepEntityLogicAsJetpack(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::jetpack

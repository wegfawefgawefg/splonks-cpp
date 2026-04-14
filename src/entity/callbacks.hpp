#pragma once

#include "audio.hpp"
#include "damage_types.hpp"

#include <cstddef>

namespace splonks {

struct Graphics;
struct State;

enum class EntityDamageEffectResult {
    None,
    Consumed,
};

using EntityOnDeath = void (*)(std::size_t entity_idx, State& state, Audio& audio);
using EntityOnDamage = EntityDamageEffectResult (*) (
    std::size_t entity_idx,
    State& state,
    Audio& audio,
    DamageType damage_type,
    unsigned int amount,
    bool damage_applied
);
using EntityOnUse = void (*)(std::size_t entity_idx, State& state, Graphics& graphics, Audio& audio);
using EntityStepLogic =
    void (*)(std::size_t entity_idx, State& state, Graphics& graphics, Audio& audio, float dt);
using EntityStepPhysics =
    void (*)(std::size_t entity_idx, State& state, Graphics& graphics, Audio& audio, float dt);

} // namespace splonks

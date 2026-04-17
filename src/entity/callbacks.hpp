#pragma once

#include "audio.hpp"
#include "damage_types.hpp"
#include "math_types.hpp"

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
using EntityOnTryBuy = bool (*) (
    std::size_t entity_idx,
    std::size_t buyer_idx,
    State& state,
    Graphics& graphics,
    Audio& audio
);
using EntityOnAreaEnter = void (*) (
    std::size_t area_idx,
    std::size_t other_idx,
    State& state,
    Graphics& graphics,
    Audio& audio
);
using EntityOnAreaExit = void (*) (
    std::size_t area_idx,
    std::size_t other_idx,
    State& state,
    Graphics& graphics,
    Audio& audio
);
using EntityOnAreaTileChanged = void (*) (
    std::size_t area_idx,
    const IVec2& tile_pos,
    State& state,
    Audio& audio
);
using EntityStepLogic =
    void (*)(std::size_t entity_idx, State& state, Graphics& graphics, Audio& audio, float dt);
using EntityStepPhysics =
    void (*)(std::size_t entity_idx, State& state, Graphics& graphics, Audio& audio, float dt);

} // namespace splonks

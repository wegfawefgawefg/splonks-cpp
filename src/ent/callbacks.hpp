#pragma once

#include "audio.hpp"
#include "damage_types.hpp"
#include "ents/common/contact_types.hpp"
#include "math_types.hpp"

#include <cstddef>
#include <cstdint>

namespace splonks {

struct Graphics;
struct State;

enum class EntDamageEffectResult {
    None,
    Consumed,
};

using EntOnDeath = void (*)(std::size_t ent_idx, State& state, Audio& audio);
using EntOnDamage = EntDamageEffectResult (*) (
    std::size_t ent_idx,
    State& state,
    Audio& audio,
    DamageType damage_type,
    std::uint32_t amount,
    bool damage_applied
);
using EntOnUse = void (*)(std::size_t ent_idx, State& state, Graphics& graphics, Audio& audio);
using EntOnInteract = bool (*) (
    std::size_t ent_idx,
    std::size_t interactor_idx,
    State& state,
    Graphics& graphics,
    Audio& audio
);
using EntOnTryBuy = bool (*) (
    std::size_t ent_idx,
    std::size_t buyer_idx,
    State& state,
    Graphics& graphics,
    Audio& audio
);
using EntOnAreaEnter = void (*) (
    std::size_t area_idx,
    std::size_t other_idx,
    State& state,
    Graphics& graphics,
    Audio& audio
);
using EntOnAreaExit = void (*) (
    std::size_t area_idx,
    std::size_t other_idx,
    State& state,
    Graphics& graphics,
    Audio& audio
);
using EntOnAreaTileChanged = void (*) (
    std::size_t area_idx,
    const IVec2& tile_pos,
    State& state,
    Audio& audio
);
using EntControlLogic =
    void (*)(std::size_t ent_idx, State& state, Graphics& graphics, Audio& audio, float dt);
using EntStepLogic =
    void (*)(std::size_t ent_idx, State& state, Graphics& graphics, Audio& audio, float dt);
using EntStepPhysics =
    void (*)(std::size_t ent_idx, State& state, Graphics& graphics, Audio& audio, float dt);

using EntOnEntContact = ents::common::ContactResult (*) (
    std::size_t ent_idx,
    std::size_t other_ent_idx,
    const ents::common::ContactContext& context,
    State& state,
    const Graphics* graphics,
    Audio* audio
);
using EntOnTileContact = ents::common::ContactResult (*) (
    std::size_t ent_idx,
    const ents::common::ContactContext& context,
    State& state
);

} // namespace splonks

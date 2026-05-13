#pragma once

#include "ent.hpp"
#include "ent/spec.hpp"
#include "state.hpp"

namespace splonks::ents::meathead {

extern const EntSpec kMeatheadSpec;
void MaybePreviewMeatheadPassive(const Ent& player, State& state);
void OnMeatheadEffectHook(
    Ent& owner,
    EffectInstance& effect,
    State& state,
    Audio* audio,
    const EffectHookContext& hook
);

} // namespace splonks::ents::meathead

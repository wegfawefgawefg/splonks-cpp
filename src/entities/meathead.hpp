#pragma once

#include "entity.hpp"
#include "entity/archetype.hpp"
#include "state.hpp"

namespace splonks::entities::meathead {

extern const EntityArchetype kMeatheadArchetype;
void MaybePreviewMeatheadPassive(const Entity& player, State& state);
void OnMeatheadEffectEvent(
    Entity& owner,
    EffectInstance& effect,
    State& state,
    Audio* audio,
    const EffectEvent& event
);

} // namespace splonks::entities::meathead

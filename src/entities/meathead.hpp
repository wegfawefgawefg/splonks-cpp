#pragma once

#include "entity.hpp"
#include "entity/archetype.hpp"
#include "state.hpp"

namespace splonks::entities::meathead {

extern const EntityArchetype kMeatheadArchetype;
void MaybePreviewMeatheadPassive(const Entity& player, State& state);
void OnEntityDeathForMeathead(std::size_t entity_idx, State& state, Audio& audio);

} // namespace splonks::entities::meathead

#pragma once

#include "entity/archetype.hpp"

namespace splonks::entities::spider {

extern const EntityArchetype kSpiderArchetype;
extern const EntityArchetype kRageSpiderArchetype;
extern const EntityArchetype kGiantSpiderArchetype;

void OnDeathAsGiantSpider(std::size_t entity_idx, State& state, Audio& audio);

} // namespace splonks::entities::spider

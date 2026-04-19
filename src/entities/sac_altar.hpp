#pragma once

#include "entity/archetype.hpp"

#include <cstddef>

namespace splonks::entities::sac_altar {

extern const EntityArchetype kSacAltarArchetype;

void OnDeathAsSacAltarPiece(std::size_t entity_idx, State& state, Audio& audio);

} // namespace splonks::entities::sac_altar

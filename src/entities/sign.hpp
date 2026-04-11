#pragma once

#include "entity_archetype.hpp"

namespace splonks {

struct Audio;
struct State;

}

namespace splonks::entities::sign {

extern const EntityArchetype kSignGeneralArchetype;
extern const EntityArchetype kSignBombArchetype;
extern const EntityArchetype kSignWeaponArchetype;
extern const EntityArchetype kSignRareArchetype;
extern const EntityArchetype kSignClothingArchetype;
extern const EntityArchetype kSignCrapsArchetype;
extern const EntityArchetype kSignKissingArchetype;

void StepEntityLogicAsSign(std::size_t entity_idx, State& state, Audio& audio);

} // namespace splonks::entities::sign

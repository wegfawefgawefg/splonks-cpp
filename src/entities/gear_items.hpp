#pragma once

#include "entity_archetype.hpp"
#include "entities/common.hpp"

namespace splonks::entities::gear_items {

extern const EntityArchetype kCapeArchetype;
extern const EntityArchetype kGlovesArchetype;
extern const EntityArchetype kSpectaclesArchetype;
extern const EntityArchetype kMittArchetype;
extern const EntityArchetype kPasteArchetype;
extern const EntityArchetype kSpringShoesArchetype;
extern const EntityArchetype kSpikeShoesArchetype;
extern const EntityArchetype kBombBoxArchetype;
extern const EntityArchetype kBombBagArchetype;
extern const EntityArchetype kCompassArchetype;
extern const EntityArchetype kParachuteArchetype;
extern const EntityArchetype kRopePileArchetype;


void StepEntityLogicAsGearItem(std::size_t entity_idx, State& state, Audio& audio);
void StepEntityPhysicsAsGearItem(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

} // namespace splonks::entities::gear_items

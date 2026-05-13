#pragma once

#include "ent/spec.hpp"
#include "ents/common/common.hpp"

namespace splonks::ents::gear_items {

extern const EntSpec kCapeSpec;
extern const EntSpec kGlovesSpec;
extern const EntSpec kSpectaclesSpec;
extern const EntSpec kMittSpec;
extern const EntSpec kPasteSpec;
extern const EntSpec kSpringShoesSpec;
extern const EntSpec kSpikeShoesSpec;
extern const EntSpec kBombBoxSpec;
extern const EntSpec kBombBagSpec;
extern const EntSpec kCompassSpec;
extern const EntSpec kParachuteSpec;
extern const EntSpec kRopePileSpec;

void StepEquippedPassiveItems(std::size_t ent_idx, State& state, Graphics& graphics);
void ClearEquippedPassiveItemVisuals(Ent& ent, State& state, const Graphics& graphics);

} // namespace splonks::ents::gear_items

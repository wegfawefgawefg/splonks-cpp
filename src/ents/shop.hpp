#pragma once

#include "ent/spec.hpp"

namespace splonks::ents::shop {

sim::FxAABB GetShopArea(const Ent& shop);
void SetShopArea(Ent& shop, sim::FxAABB area);
void AddShopChild(Ent& shop, VID child_vid);
void DisturbShop(std::size_t shop_idx, State& state, Audio& audio);
void DisturbShopByVid(std::optional<VID> shop_vid, State& state, Audio& audio);
void OnShopAreaEnter(
    std::size_t area_idx,
    std::size_t other_idx,
    State& state,
    Graphics& graphics,
    Audio& audio
);

void StepEntLogicAsShop(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

extern const EntSpec kShopSpec;

} // namespace splonks::ents::shop

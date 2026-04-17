#pragma once

#include "entity/archetype.hpp"

namespace splonks::entities::shop {

AABB GetShopArea(const Entity& shop);
void SetShopArea(Entity& shop, const AABB& area);
void AddShopChild(Entity& shop, VID child_vid);
void DisturbShop(std::size_t shop_idx, State& state, Audio& audio);
void DisturbShopByVid(std::optional<VID> shop_vid, State& state, Audio& audio);
void OnShopAreaEnter(
    std::size_t area_idx,
    std::size_t other_idx,
    State& state,
    Graphics& graphics,
    Audio& audio
);

void StepEntityLogicAsShop(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

extern const EntityArchetype kShopArchetype;

} // namespace splonks::entities::shop

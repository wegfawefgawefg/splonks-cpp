#pragma once

#include "entity/core_types.hpp"
#include "quest.hpp"
#include "stage.hpp"
#include "stage_gen/classic/room_templates.hpp"

#include <vector>

namespace splonks::stage_gen::classic {

EntityType PickWeightedEntity(const std::vector<WeightedEntityEntry>& entries);
EntityType PickUndergroundItemType(const ItemPoolDb& item_db, const Stage& stage);
EntityType PickHighEndShopItemType(const ItemPoolDb& item_db, const Stage& stage,
                                   const std::vector<StageEntitySpawn>& room_spawns);
EntityType PickShopItemType(ShopType shop_type, const Stage& stage,
                            const std::vector<StageEntitySpawn>& room_spawns,
                            const ItemPoolDb& item_db, const ShopConfigDb& shop_db);

} // namespace splonks::stage_gen::classic

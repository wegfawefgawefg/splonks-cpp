#pragma once

#include "ent/core_types.hpp"
#include "quest.hpp"
#include "stage.hpp"
#include "stage_gen/classic/room_templates.hpp"

#include <vector>

namespace splonks::stage_gen::classic {

EntType PickWeightedEnt(const std::vector<WeightedEntEntry>& entries);
EntType PickUndergroundItemType(const ItemPoolDb& item_db, const Stage& stage);
EntType PickHighEndShopItemType(const ItemPoolDb& item_db, const Stage& stage,
                                   const std::vector<EntSpawn>& room_spawns);
EntType PickShopItemType(ShopType shop_type, const Stage& stage,
                            const std::vector<EntSpawn>& room_spawns,
                            const ItemPoolDb& item_db, const ShopConfigDb& shop_db);

} // namespace splonks::stage_gen::classic

#include "stage_gen/classic/item_pools.hpp"

#include "utils.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <string_view>

namespace splonks::stage_gen::classic {

namespace {

bool HasSpawnType(const Stage& stage, EntityType type_) {
    for (const StageEntitySpawn& spawn : stage.entity_spawns) {
        if (spawn.type_ == type_) {
            return true;
        }
    }
    return false;
}

bool HasSpawnType(const std::vector<StageEntitySpawn>& spawns, EntityType type_) {
    for (const StageEntitySpawn& spawn : spawns) {
        if (spawn.type_ == type_) {
            return true;
        }
    }
    return false;
}

bool HasSpawnType(const Stage& stage, const std::vector<StageEntitySpawn>& spawns,
                  EntityType type_) {
    return HasSpawnType(stage, type_) || HasSpawnType(spawns, type_);
}

EntityType PickWeightedEntityInternal(const std::vector<WeightedEntityEntry>& entries) {
    int total_weight = 0;
    for (const WeightedEntityEntry& entry : entries) {
        total_weight += std::max(0, entry.weight);
    }
    if (total_weight <= 0) {
        return EntityType::None;
    }
    int roll = rng::RandomIntInclusive(1, total_weight);
    for (const WeightedEntityEntry& entry : entries) {
        roll -= std::max(0, entry.weight);
        if (roll <= 0) {
            return entry.entity_type;
        }
    }
    return entries.empty() ? EntityType::None : entries.back().entity_type;
}

EntityType PickEntityFromPool(const EntityPoolConfig* pool, std::string_view pool_id,
                              const Stage& stage,
                              const std::vector<StageEntitySpawn>& room_spawns) {
    if (pool == nullptr) {
        throw std::runtime_error("Classic item pool is not configured: " +
                                 std::string(pool_id));
    }

    std::vector<WeightedEntityEntry> candidates;
    candidates.reserve(pool->entries.size());
    for (const WeightedEntityEntry& entry : pool->entries) {
        if (entry.entity_type == EntityType::None) {
            continue;
        }
        if (pool->unique && HasSpawnType(stage, room_spawns, entry.entity_type)) {
            continue;
        }
        candidates.push_back(entry);
    }

    const EntityType picked = PickWeightedEntityInternal(candidates);
    if (picked != EntityType::None) {
        return picked;
    }
    throw std::runtime_error("Classic item pool has no eligible entries: " +
                             std::string(pool_id));
}

EntityType PickEntityFromPool(const ItemPoolDb& item_db, std::string_view pool_id,
                              const Stage& stage,
                              const std::vector<StageEntitySpawn>& room_spawns) {
    return PickEntityFromPool(item_db.FindPool(pool_id), pool_id, stage, room_spawns);
}

} // namespace

EntityType PickWeightedEntity(const std::vector<WeightedEntityEntry>& entries) {
    return PickWeightedEntityInternal(entries);
}

EntityType PickUndergroundItemType(const ItemPoolDb& item_db, const Stage& stage) {
    static const std::vector<StageEntitySpawn> kNoRoomSpawns;
    return PickEntityFromPool(item_db, "underground_items", stage, kNoRoomSpawns);
}

EntityType PickHighEndShopItemType(const ItemPoolDb& item_db, const Stage& stage,
                                   const std::vector<StageEntitySpawn>& room_spawns) {
    return PickEntityFromPool(item_db, "high_end_shop_items", stage, room_spawns);
}

EntityType PickShopItemType(ShopType shop_type, const Stage& stage,
                            const std::vector<StageEntitySpawn>& room_spawns,
                            const ItemPoolDb& item_db, const ShopConfigDb& shop_db) {
    const ShopTypeConfig* shop_config = shop_db.FindShopType(ShopTypeId(shop_type));
    if (shop_config == nullptr) {
        return EntityType::None;
    }
    return PickEntityFromPool(item_db, shop_config->item_pool, stage, room_spawns);
}

} // namespace splonks::stage_gen::classic

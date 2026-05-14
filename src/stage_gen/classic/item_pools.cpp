#include "stage_gen/classic/item_pools.hpp"

#include "utils.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <string_view>

namespace splonks::stage_gen::classic {

namespace {

bool HasSpawnType(const Stage& stage, EntType type_) {
    for (const EntSpawn& spawn : stage.ent_spawns) {
        if (spawn.type_ == type_) {
            return true;
        }
    }
    return false;
}

bool HasSpawnType(const std::vector<EntSpawn>& spawns, EntType type_) {
    for (const EntSpawn& spawn : spawns) {
        if (spawn.type_ == type_) {
            return true;
        }
    }
    return false;
}

bool HasSpawnType(const Stage& stage, const std::vector<EntSpawn>& spawns,
                  EntType type_) {
    return HasSpawnType(stage, type_) || HasSpawnType(spawns, type_);
}

EntType PickWeightedEntInternal(const std::vector<WeightedEntEntry>& entries, DetRng& det_rng) {
    int total_weight = 0;
    for (const WeightedEntEntry& entry : entries) {
        total_weight += std::max(0, entry.weight);
    }
    if (total_weight <= 0) {
        return EntType::None;
    }
    int roll = det_rng.RandomIntInclusive(1, total_weight);
    for (const WeightedEntEntry& entry : entries) {
        roll -= std::max(0, entry.weight);
        if (roll <= 0) {
            return entry.ent_type;
        }
    }
    return entries.empty() ? EntType::None : entries.back().ent_type;
}

EntType PickEntFromPool(const EntPoolConfig* pool, std::string_view pool_id,
                              const Stage& stage,
                              const std::vector<EntSpawn>& room_spawns,
                              DetRng& det_rng) {
    if (pool == nullptr) {
        throw std::runtime_error("Classic item pool is not configured: " +
                                 std::string(pool_id));
    }

    std::vector<WeightedEntEntry> candidates;
    candidates.reserve(pool->entries.size());
    for (const WeightedEntEntry& entry : pool->entries) {
        if (entry.ent_type == EntType::None) {
            continue;
        }
        if ((pool->unique || entry.unique) &&
            HasSpawnType(stage, room_spawns, entry.ent_type)) {
            continue;
        }
        candidates.push_back(entry);
    }

    const EntType picked = PickWeightedEntInternal(candidates, det_rng);
    if (picked != EntType::None) {
        return picked;
    }
    throw std::runtime_error("Classic item pool has no eligible entries: " +
                             std::string(pool_id));
}

EntType PickEntFromPool(const ItemPoolDb& item_db, std::string_view pool_id,
                              const Stage& stage,
                              const std::vector<EntSpawn>& room_spawns,
                              DetRng& det_rng) {
    return PickEntFromPool(item_db.FindPool(pool_id), pool_id, stage, room_spawns, det_rng);
}

} // namespace

EntType PickWeightedEnt(const std::vector<WeightedEntEntry>& entries, DetRng& det_rng) {
    return PickWeightedEntInternal(entries, det_rng);
}

EntType PickUndergroundItemType(const ItemPoolDb& item_db, const Stage& stage, DetRng& det_rng) {
    static const std::vector<EntSpawn> kNoRoomSpawns;
    return PickEntFromPool(item_db, "underground_items", stage, kNoRoomSpawns, det_rng);
}

EntType PickHighEndShopItemType(const ItemPoolDb& item_db, const Stage& stage,
                                const std::vector<EntSpawn>& room_spawns, DetRng& det_rng) {
    return PickEntFromPool(item_db, "high_end_shop_items", stage, room_spawns, det_rng);
}

EntType PickShopItemType(ShopType shop_type, const Stage& stage,
                            const std::vector<EntSpawn>& room_spawns,
                            const ItemPoolDb& item_db, const ShopConfigDb& shop_db,
                            DetRng& det_rng) {
    const ShopTypeConfig* shop_config = shop_db.FindShopType(ShopTypeId(shop_type));
    if (shop_config == nullptr) {
        return EntType::None;
    }
    return PickEntFromPool(item_db, shop_config->item_pool, stage, room_spawns, det_rng);
}

} // namespace splonks::stage_gen::classic

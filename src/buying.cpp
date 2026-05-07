#include "buying.hpp"

#include "entities/common/common.hpp"
#include "frame_data_id.hpp"
#include "player_queries.hpp"
#include "state.hpp"
#include "world_ops.hpp"
#include "world_query.hpp"

#include <algorithm>
#include <limits>
#include <vector>

namespace splonks {

namespace {

Vec2 GetAabbCenter(const AABB& aabb) {
    return (aabb.tl + aabb.br) / 2.0F;
}

float GetBuyPromptDistanceSq(const Vec2& buyer_center, const Vec2& item_center, const Stage& stage) {
    const Vec2 delta = GetNearestWorldDelta(stage, buyer_center, item_center);
    return delta.x * delta.x + delta.y * delta.y;
}

struct OverlappingBuyableEntity {
    std::size_t entity_idx = 0;
    float distance_sq = 0.0F;
    AABB nearest_aabb = AABB::New(Vec2::New(0.0F, 0.0F), Vec2::New(0.0F, 0.0F));
};

std::vector<OverlappingBuyableEntity> FindOverlappingBuyableEntities(
    const State& state,
    const Graphics& graphics,
    std::size_t buyer_idx
) {
    if (buyer_idx >= state.entity_manager.entities.size()) {
        return {};
    }

    const Entity& buyer = state.entity_manager.entities[buyer_idx];
    if (!buyer.active) {
        return {};
    }

    const AABB buyer_aabb = entities::common::GetContactAabbForEntity(buyer, graphics);
    const Vec2 buyer_center = GetAabbCenter(buyer_aabb);
    const std::vector<VID> results = QueryEntitiesInAabb(state, buyer_aabb, buyer.vid);

    std::vector<OverlappingBuyableEntity> overlaps;
    overlaps.reserve(results.size());
    for (const VID& vid : results) {
        const Entity* const item = state.entity_manager.GetEntity(vid);
        if (item == nullptr || !item->active || !item->buyable.active) {
            continue;
        }

        const AABB item_aabb = GetNearestWorldAabb(
            state.stage,
            buyer_center,
            entities::common::GetContactAabbForEntity(*item, graphics)
        );
        if (!AabbsIntersect(buyer_aabb, item_aabb)) {
            continue;
        }

        overlaps.push_back(OverlappingBuyableEntity{
            .entity_idx = vid.id,
            .distance_sq = GetBuyPromptDistanceSq(buyer_center, GetAabbCenter(item_aabb), state.stage),
            .nearest_aabb = item_aabb,
        });
    }

    std::sort(
        overlaps.begin(),
        overlaps.end(),
        [](const OverlappingBuyableEntity& a, const OverlappingBuyableEntity& b) {
            if (a.distance_sq != b.distance_sq) {
                return a.distance_sq < b.distance_sq;
            }
            return a.entity_idx < b.entity_idx;
        }
    );
    return overlaps;
}

} // namespace

std::optional<std::size_t> FindOverlappingBuyableEntityIdx(
    const State& state,
    const Graphics& graphics,
    std::size_t buyer_idx
) {
    const std::vector<OverlappingBuyableEntity> overlaps =
        FindOverlappingBuyableEntities(state, graphics, buyer_idx);
    if (overlaps.empty()) {
        return std::nullopt;
    }
    return overlaps.front().entity_idx;
}

void ClearEntityBuyableState(Entity& entity) {
    entity.buyable.active = false;
    entity.buyable.display_quantity = 0;
    entity.buyable.display_icon_animation_id.reset();
    entity.buyable.shop_owner_vid.reset();
    entity.buyable.on_try_buy = nullptr;
}

void ConfigureEntityAsBuyable(Entity& entity, std::uint32_t price) {
    entity.buyable.active = true;
    entity.buyable.display_quantity = price;
    entity.buyable.display_icon_animation_id = frame_data_ids::GoldIcon;
    if (entity.buyable.on_try_buy == nullptr) {
        entity.buyable.on_try_buy = TryBuyEntityForMoney;
    }
}

bool TrySpendMoney(std::size_t buyer_idx, std::uint32_t amount, State& state, Audio& audio) {
    (void)audio;
    if (buyer_idx >= state.entity_manager.entities.size()) {
        return false;
    }

    Entity& buyer = state.entity_manager.entities[buyer_idx];
    if (!buyer.active || buyer.money < amount) {
        return false;
    }

    buyer.money -= amount;
    world_ops::PatchPlayerState(state, buyer);
    if (amount > 0) {
        (void)PlayEntityCenterSoundEmitter(state, buyer, audio_asset_ids::Gold);
    }
    return true;
}

bool TryBuyEntityForMoney(
    std::size_t entity_idx,
    std::size_t buyer_idx,
    State& state,
    Graphics& graphics,
    Audio& audio
) {
    if (entity_idx >= state.entity_manager.entities.size() ||
        buyer_idx >= state.entity_manager.entities.size()) {
        return false;
    }

    Entity& buyer = state.entity_manager.entities[buyer_idx];
    Entity& item = state.entity_manager.entities[entity_idx];
    if (!buyer.active || !item.active || !item.buyable.active) {
        return false;
    }
    if (!TrySpendMoney(buyer_idx, item.buyable.display_quantity, state, audio)) {
        return false;
    }

    ClearEntityBuyableState(item);
    world_ops::PatchEntityState(state, item, item);
    world_ops::PatchPlayerState(state, buyer);
    if (TryCollectInventoryPickup(state, buyer, item)) {
        (void)world_ops::DeactivateEntity(state, item.vid);
        state.UpdateSidForEntity(entity_idx, graphics);
    }
    return true;
}

void AddBuyPromptsForPlayer(State& state, const Graphics& graphics) {
    const std::optional<VID> buyer_vid = FindPrimaryLocalPlayerVid(state);
    if (!buyer_vid.has_value()) {
        return;
    }
    const std::size_t buyer_idx = buyer_vid->id;
    if (buyer_idx >= state.entity_manager.entities.size()) {
        return;
    }

    const Entity& buyer = state.entity_manager.entities[buyer_idx];
    if (!buyer.active) {
        return;
    }

    const std::vector<OverlappingBuyableEntity> overlaps =
        FindOverlappingBuyableEntities(state, graphics, buyer_idx);
    for (const OverlappingBuyableEntity& overlap : overlaps) {
        const Entity& item = state.entity_manager.entities[overlap.entity_idx];
        state.AddWorldPrompt(WorldPrompt{
            .world_pos = Vec2::New((overlap.nearest_aabb.tl.x + overlap.nearest_aabb.br.x) * 0.5F, overlap.nearest_aabb.tl.y - 6.0F),
            .action_text = "RB",
            .message_text = "",
            .show_down_arrow = false,
            .quantity = item.buyable.display_quantity,
            .icon_animation_id = item.buyable.display_icon_animation_id,
        });
    }
}

bool TryBuyEntity(
    std::size_t entity_idx,
    std::size_t buyer_idx,
    State& state,
    Graphics& graphics,
    Audio& audio
) {
    if (entity_idx >= state.entity_manager.entities.size() ||
        buyer_idx >= state.entity_manager.entities.size()) {
        return false;
    }

    const Entity& item = state.entity_manager.entities[entity_idx];
    if (!item.active || !item.buyable.active || item.buyable.on_try_buy == nullptr) {
        return false;
    }

    return item.buyable.on_try_buy(entity_idx, buyer_idx, state, graphics, audio);
}

} // namespace splonks

#include "buying.hpp"

#include "entities/common/common.hpp"
#include "state.hpp"
#include "world_query.hpp"

#include <limits>

namespace splonks {

namespace {

Vec2 GetAabbCenter(const AABB& aabb) {
    return (aabb.tl + aabb.br) / 2.0F;
}

float GetBuyPromptDistanceSq(const Vec2& buyer_center, const Vec2& item_center, const Stage& stage) {
    const Vec2 delta = GetNearestWorldDelta(stage, buyer_center, item_center);
    return delta.x * delta.x + delta.y * delta.y;
}

std::optional<std::size_t> FindOverlappingBuyableEntity(
    const State& state,
    const Graphics& graphics,
    std::size_t buyer_idx
) {
    if (buyer_idx >= state.entity_manager.entities.size()) {
        return std::nullopt;
    }

    const Entity& buyer = state.entity_manager.entities[buyer_idx];
    if (!buyer.active) {
        return std::nullopt;
    }

    const AABB buyer_aabb = entities::common::GetContactAabbForEntity(buyer, graphics);
    const Vec2 buyer_center = GetAabbCenter(buyer_aabb);
    const std::vector<VID> results = QueryEntitiesInAabb(state, buyer_aabb, buyer.vid);

    float best_distance_sq = std::numeric_limits<float>::max();
    std::optional<std::size_t> best_entity_idx;
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

        const float distance_sq = GetBuyPromptDistanceSq(
            buyer_center,
            GetAabbCenter(item_aabb),
            state.stage
        );
        if (!best_entity_idx.has_value() || distance_sq < best_distance_sq) {
            best_distance_sq = distance_sq;
            best_entity_idx = vid.id;
        }
    }
    return best_entity_idx;
}

} // namespace

void ClearEntityBuyableState(Entity& entity) {
    entity.buyable.active = false;
    entity.buyable.display_quantity = 0;
    entity.buyable.display_icon_animation_id.reset();
    entity.buyable.shop_owner_vid.reset();
    entity.buyable.on_try_buy = nullptr;
}

bool TrySpendMoney(std::size_t buyer_idx, std::uint32_t amount, State& state, Audio& audio) {
    if (buyer_idx >= state.entity_manager.entities.size()) {
        return false;
    }

    Entity& buyer = state.entity_manager.entities[buyer_idx];
    if (!buyer.active || buyer.money < amount) {
        return false;
    }

    buyer.money -= amount;
    if (amount > 0) {
        audio.PlaySoundEffect(SoundEffect::Gold);
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
    if (TryCollectPassiveItem(buyer, item)) {
        state.entity_manager.SetInactive(entity_idx);
        state.UpdateSidForEntity(entity_idx, graphics);
    }
    return true;
}

std::optional<BuyPrompt> GetBuyPromptForPlayer(const State& state, const Graphics& graphics) {
    if (!state.player_vid.has_value()) {
        return std::nullopt;
    }
    const std::size_t buyer_idx = state.player_vid->id;
    if (buyer_idx >= state.entity_manager.entities.size()) {
        return std::nullopt;
    }

    const Entity& buyer = state.entity_manager.entities[buyer_idx];
    if (!buyer.active) {
        return std::nullopt;
    }

    const std::optional<std::size_t> item_idx = FindOverlappingBuyableEntity(state, graphics, buyer_idx);
    if (!item_idx.has_value()) {
        return std::nullopt;
    }

    const Entity& item = state.entity_manager.entities[*item_idx];
    return BuyPrompt{
        .entity_idx = *item_idx,
        .quantity = item.buyable.display_quantity,
        .icon_animation_id = item.buyable.display_icon_animation_id,
    };
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

bool TryBuyOverlappingEntity(std::size_t buyer_idx, State& state, Graphics& graphics, Audio& audio) {
    const std::optional<std::size_t> item_idx = FindOverlappingBuyableEntity(state, graphics, buyer_idx);
    if (!item_idx.has_value()) {
        return false;
    }
    return TryBuyEntity(*item_idx, buyer_idx, state, graphics, audio);
}

} // namespace splonks

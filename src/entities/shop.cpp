#include "entities/shop.hpp"

#include "buying.hpp"
#include "frame_data_animator.hpp"
#include "frame_data_id.hpp"
#include "player_queries.hpp"
#include "state.hpp"
#include "world_ops.hpp"
#include "world_query.hpp"

#include <algorithm>
#include <vector>

namespace splonks::entities::shop {

namespace {

std::vector<VID>& EnsureChildVids(Entity& shop) {
    if (!shop.child_vids.has_value()) {
        shop.child_vids.emplace();
    }
    return *shop.child_vids;
}

bool ContainsVid(const std::vector<VID>& vids, VID vid) {
    return std::find(vids.begin(), vids.end(), vid) != vids.end();
}

void ClearOwnedBuyableIfPresent(Entity& entity, VID shop_vid) {
    if (!entity.buyable.active || !entity.buyable.shop_owner_vid.has_value() ||
        *entity.buyable.shop_owner_vid != shop_vid) {
        return;
    }
    ClearEntityBuyableState(entity);
}

} // namespace

AABB GetShopArea(const Entity& shop) {
    return shop.GetAABB();
}

void SetShopArea(Entity& shop, const AABB& area) {
    shop.pos = area.tl;
    shop.size = area.br - area.tl + Vec2::New(1.0F, 1.0F);
    shop.point_a = IVec2::New(0, 0);
    shop.point_b = IVec2::New(0, 0);
    shop.point_label_a = PointLabel::None;
    shop.point_label_b = PointLabel::None;
}

void AddShopChild(Entity& shop, VID child_vid) {
    std::vector<VID>& child_vids = EnsureChildVids(shop);
    if (!ContainsVid(child_vids, child_vid)) {
        child_vids.push_back(child_vid);
    }
}

void DisturbShop(std::size_t shop_idx, State& state, Audio& audio) {
    (void)audio;
    if (shop_idx >= state.entity_manager.entities.size()) {
        return;
    }

    Entity& shop = state.entity_manager.entities[shop_idx];
    if (!shop.active || shop.type_ != EntityType::Shop) {
        return;
    }

    const bool already_disturbed = shop.ai_state == EntityAiState::Disturbed;
    shop.ai_state = EntityAiState::Disturbed;
    world_ops::PatchEntityState(state, shop, shop);

    if (!already_disturbed) {
        (void)PlayEntityCenterSoundEmitter(state, shop, audio_asset_ids::ShopkeepAnger0);
    }

    for (const PlayerSlot& slot : state.players.slots) {
        if (!slot.connected || !slot.entity_vid.has_value()) {
            continue;
        }
        if (Entity* const player = state.entity_manager.GetEntityMut(*slot.entity_vid);
            player != nullptr && player->active) {
            const bool was_wanted = player->wanted;
            player->wanted = true;
            if (!was_wanted) {
                world_ops::PatchPlayerState(state, *player);
            }
        }
    }

    if (shop.entity_a.has_value()) {
        if (Entity* const shopkeeper = state.entity_manager.GetEntityMut(*shop.entity_a)) {
            const bool was_wanted = shopkeeper->wanted;
            shopkeeper->wanted = true;
            if (!was_wanted) {
                world_ops::PatchEntityState(state, shop, *shopkeeper);
            }
        }
    }

    if (already_disturbed || !shop.child_vids.has_value()) {
        return;
    }

    for (const VID& child_vid : *shop.child_vids) {
        Entity* const child = state.entity_manager.GetEntityMut(child_vid);
        if (child == nullptr || !child->active) {
            continue;
        }
        ClearOwnedBuyableIfPresent(*child, shop.vid);
        world_ops::PatchEntityState(state, shop, *child);
    }

}

void DisturbShopByVid(std::optional<VID> shop_vid, State& state, Audio& audio) {
    if (!shop_vid.has_value()) {
        return;
    }
    if (shop_vid->id >= state.entity_manager.entities.size()) {
        return;
    }
    DisturbShop(shop_vid->id, state, audio);
}

void OnShopAreaEnter(
    std::size_t area_idx,
    std::size_t other_idx,
    State& state,
    Graphics& graphics,
    Audio& audio
) {
    (void)graphics;

    if (area_idx >= state.entity_manager.entities.size() ||
        other_idx >= state.entity_manager.entities.size()) {
        return;
    }

    const Entity& shop = state.entity_manager.entities[area_idx];
    const Entity& other = state.entity_manager.entities[other_idx];
    if (!shop.active || shop.type_ != EntityType::Shop || !other.active) {
        return;
    }

    if (other.type_ == EntityType::Block) {
        DisturbShop(area_idx, state, audio);
        return;
    }

    if (!IsPlayerLikeEntityType(other.type_)) {
        return;
    }

    (void)PlayEntityCenterSoundEmitter(state, shop, audio_asset_ids::LawsonEnter);
}

void StepEntityLogicAsShop(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)dt;

    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    Entity& shop = state.entity_manager.entities[entity_idx];
    if (!shop.active || shop.type_ != EntityType::Shop ||
        shop.ai_state == EntityAiState::Disturbed ||
        !shop.child_vids.has_value()) {
        return;
    }

    const AABB shop_area = GetShopArea(shop);
    for (const VID& child_vid : *shop.child_vids) {
        Entity* const child = state.entity_manager.GetEntityMut(child_vid);
        if (child == nullptr || !child->active) {
            continue;
        }
        if (!child->buyable.active || !child->buyable.shop_owner_vid.has_value() ||
            *child->buyable.shop_owner_vid != shop.vid) {
            continue;
        }

        const AABB child_aabb = child->GetAABB();
        if (WorldAabbsIntersect(state.stage, child_aabb, shop_area)) {
            continue;
        }

        DisturbShop(entity_idx, state, audio);
        return;
    }
}

extern const EntityArchetype kShopArchetype{
    .type_ = EntityType::Shop,
    .size = Vec2::New(1.0F, 1.0F),
    .health = 1,
    .has_physics = false,
    .can_collide = false,
    .can_be_hit = false,
    .can_be_picked_up = false,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_go_on_back = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Background,
    .render_enabled = false,
    .condition = EntityCondition::Normal,
    .ai_state = EntityAiState::Idle,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Immune,
    .on_area_enter = OnShopAreaEnter,
    .step_logic = StepEntityLogicAsShop,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::NoSprite),
};

} // namespace splonks::entities::shop

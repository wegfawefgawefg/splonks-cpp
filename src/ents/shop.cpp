#include "ents/shop.hpp"

#include "buying.hpp"
#include "aframe_animator.hpp"
#include "aframe_id.hpp"
#include "player_queries.hpp"
#include "state.hpp"
#include "world_ops.hpp"
#include "world_query.hpp"

#include <algorithm>
#include <vector>

namespace splonks::ents::shop {

namespace {

std::vector<VID>& EnsureChildVids(Ent& shop) {
    if (!shop.child_vids.has_value()) {
        shop.child_vids.emplace();
    }
    return *shop.child_vids;
}

bool ContainsVid(const std::vector<VID>& vids, VID vid) {
    return std::find(vids.begin(), vids.end(), vid) != vids.end();
}

void ClearOwnedBuyableIfPresent(Ent& ent, VID shop_vid) {
    if (!ent.buyable.active || !ent.buyable.shop_owner_vid.has_value() ||
        *ent.buyable.shop_owner_vid != shop_vid) {
        return;
    }
    ClearEntBuyableState(ent);
}

} // namespace

AABB GetShopArea(const Ent& shop) {
    return shop.GetAABB();
}

void SetShopArea(Ent& shop, const AABB& area) {
    shop.pos = area.tl;
    shop.size = sim::ToSimVec2(area.br - area.tl + Vec2::New(1.0F, 1.0F));
    shop.point_a = IVec2::New(0, 0);
    shop.point_b = IVec2::New(0, 0);
    shop.point_label_a = PointLabel::None;
    shop.point_label_b = PointLabel::None;
}

void AddShopChild(Ent& shop, VID child_vid) {
    std::vector<VID>& child_vids = EnsureChildVids(shop);
    if (!ContainsVid(child_vids, child_vid)) {
        child_vids.push_back(child_vid);
    }
}

void DisturbShop(std::size_t shop_idx, State& state, Audio& audio) {
    (void)audio;
    if (shop_idx >= state.ents.ents.size()) {
        return;
    }

    Ent& shop = state.ents.ents[shop_idx];
    if (!shop.active || shop.type_ != EntType::Shop) {
        return;
    }

    const bool already_disturbed = shop.ai_state == EntAiState::Disturbed;
    shop.ai_state = EntAiState::Disturbed;

    if (!already_disturbed) {
        (void)PlayEntCenterSoundEmitter(state, shop, audio_asset_ids::ShopkeepAnger0);
    }

    for (const PlayerSlot& slot : state.players.slots) {
        if (!slot.connected || !slot.ent_vid.has_value()) {
            continue;
        }
        if (Ent* const player = state.ents.GetEntMut(*slot.ent_vid);
            player != nullptr && player->active) {
            const bool was_wanted = player->wanted;
            player->wanted = true;
            (void)was_wanted;
        }
    }

    if (shop.ent_a.has_value()) {
        if (Ent* const shopkeeper = state.ents.GetEntMut(*shop.ent_a)) {
            const bool was_wanted = shopkeeper->wanted;
            shopkeeper->wanted = true;
            (void)was_wanted;
        }
    }

    if (already_disturbed || !shop.child_vids.has_value()) {
        return;
    }

    for (const VID& child_vid : *shop.child_vids) {
        Ent* const child = state.ents.GetEntMut(child_vid);
        if (child == nullptr || !child->active) {
            continue;
        }
        ClearOwnedBuyableIfPresent(*child, shop.vid);
    }

}

void DisturbShopByVid(std::optional<VID> shop_vid, State& state, Audio& audio) {
    if (!shop_vid.has_value()) {
        return;
    }
    if (shop_vid->id >= state.ents.ents.size()) {
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

    if (area_idx >= state.ents.ents.size() ||
        other_idx >= state.ents.ents.size()) {
        return;
    }

    const Ent& shop = state.ents.ents[area_idx];
    const Ent& other = state.ents.ents[other_idx];
    if (!shop.active || shop.type_ != EntType::Shop || !other.active) {
        return;
    }

    if (other.type_ == EntType::Block) {
        DisturbShop(area_idx, state, audio);
        return;
    }

    if (!IsPlayerLikeEntType(other.type_)) {
        return;
    }

    (void)PlayEntCenterSoundEmitter(state, shop, audio_asset_ids::LawsonEnter);
}

void StepEntLogicAsShop(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)dt;

    if (ent_idx >= state.ents.ents.size()) {
        return;
    }

    Ent& shop = state.ents.ents[ent_idx];
    if (!shop.active || shop.type_ != EntType::Shop ||
        shop.ai_state == EntAiState::Disturbed ||
        !shop.child_vids.has_value()) {
        return;
    }

    const AABB shop_area = GetShopArea(shop);
    for (const VID& child_vid : *shop.child_vids) {
        Ent* const child = state.ents.GetEntMut(child_vid);
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

        DisturbShop(ent_idx, state, audio);
        return;
    }
}

extern const EntSpec kShopSpec{
    .type_ = EntType::Shop,
    .size = EntSpecSize(1.0F, 1.0F),
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
    .condition = EntCondition::Normal,
    .ai_state = EntAiState::Idle,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::Immune,
    .on_area_enter = OnShopAreaEnter,
    .step_logic = StepEntLogicAsShop,
    .aframe_animator = AFrameAnimator::New(aframe_ids::NoSprite),
};

} // namespace splonks::ents::shop

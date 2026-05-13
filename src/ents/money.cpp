#include "ents/money.hpp"

#include "audio_emitters.hpp"
#include "ents/common/common.hpp"
#include "effects/treasure_pickup.hpp"

#include "ent/spec.hpp"
#include "ent/core_types.hpp"
#include "aframe_animator.hpp"
#include "aframe_id.hpp"
#include "math_types.hpp"
#include "world_ops.hpp"

namespace splonks::ents::money {

namespace {

constexpr Color3 kGoldLightColor = Color3::New(1.0F, 0.78F, 0.24F);

common::ContactResult TryCollectMoneyPickup(
    std::size_t pickup_idx,
    std::size_t collector_idx,
    State& state,
    const Graphics* graphics,
    Audio* audio,
    unsigned int amount,
    AudioAssetId sound,
    int sparkle_count
) {
    if (graphics == nullptr || audio == nullptr ||
        !common::CanCollectPickupFromContact(pickup_idx, collector_idx, state)) {
        return common::ContactResult{};
    }
    Ent& collector = state.ents.ents[collector_idx];
    const Ent& pickup = state.ents.ents[pickup_idx];
    collector.money += amount;
    (void)PlayEntCenterSoundEmitter(state, pickup, sound);
    effects::SpawnTreasurePickupSparkles(pickup, state, kGoldLightColor, sparkle_count);
    common::DeactivateCollectedPickup(pickup_idx, state, *graphics);
    return common::ContactResult{};
}

common::ContactResult OnEntContactAsGold(
    std::size_t ent_idx,
    std::size_t other_ent_idx,
    const common::ContactContext&,
    State& state,
    const Graphics* graphics,
    Audio* audio
) {
    return TryCollectMoneyPickup(ent_idx, other_ent_idx, state, graphics, audio, 500, audio_asset_ids::Gold, 2);
}

common::ContactResult OnEntContactAsGoldStack(
    std::size_t ent_idx,
    std::size_t other_ent_idx,
    const common::ContactContext&,
    State& state,
    const Graphics* graphics,
    Audio* audio
) {
    return TryCollectMoneyPickup(ent_idx, other_ent_idx, state, graphics, audio, 1500, audio_asset_ids::GoldStack, 3);
}

common::ContactResult OnEntContactAsGoldChunk(
    std::size_t ent_idx,
    std::size_t other_ent_idx,
    const common::ContactContext&,
    State& state,
    const Graphics* graphics,
    Audio* audio
) {
    return TryCollectMoneyPickup(ent_idx, other_ent_idx, state, graphics, audio, 100, audio_asset_ids::Gold, 3);
}

common::ContactResult OnEntContactAsGoldNugget(
    std::size_t ent_idx,
    std::size_t other_ent_idx,
    const common::ContactContext&,
    State& state,
    const Graphics* graphics,
    Audio* audio
) {
    return TryCollectMoneyPickup(ent_idx, other_ent_idx, state, graphics, audio, 500, audio_asset_ids::GoldStack, 5);
}

common::ContactResult OnEntContactAsGoldBar(
    std::size_t ent_idx,
    std::size_t other_ent_idx,
    const common::ContactContext&,
    State& state,
    const Graphics* graphics,
    Audio* audio
) {
    return TryCollectMoneyPickup(ent_idx, other_ent_idx, state, graphics, audio, 500, audio_asset_ids::GoldStack, 5);
}

common::ContactResult OnEntContactAsGoldBars(
    std::size_t ent_idx,
    std::size_t other_ent_idx,
    const common::ContactContext&,
    State& state,
    const Graphics* graphics,
    Audio* audio
) {
    return TryCollectMoneyPickup(ent_idx, other_ent_idx, state, graphics, audio, 1500, audio_asset_ids::GoldStack, 7);
}

} // namespace

extern const EntSpec kGoldSpec{
    .type_ = EntType::Gold,
    .size = Vec2::New(5.0F, 5.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .buoyancy = 0.55F,
    .self_light = 0.15F,
    .light_strength = 0.30F,
    .light_color = kGoldLightColor,
    .light_radius = 3,
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::CrushingOnly,
    .proj_contact_damage_amount = 0,
    .can_apply_proj_contact = false,
    .on_ent_contact = OnEntContactAsGold,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::GoldCoin),
};

extern const EntSpec kGoldStackSpec{
    .type_ = EntType::GoldStack,
    .size = Vec2::New(10.0F, 6.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .buoyancy = 0.55F,
    .self_light = 0.15F,
    .light_strength = 0.30F,
    .light_color = kGoldLightColor,
    .light_radius = 3,
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::CrushingOnly,
    .proj_contact_damage_amount = 0,
    .can_apply_proj_contact = false,
    .on_ent_contact = OnEntContactAsGoldStack,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::GoldStack),
};

extern const EntSpec kGoldChunkSpec{
    .type_ = EntType::GoldChunk,
    .size = Vec2::New(5.0F, 5.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .buoyancy = 0.35F,
    .self_light = 0.12F,
    .light_strength = 0.14F,
    .light_color = kGoldLightColor,
    .light_radius = 2,
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::CrushingOnly,
    .proj_contact_damage_amount = 0,
    .can_apply_proj_contact = false,
    .on_ent_contact = OnEntContactAsGoldChunk,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::GoldChunk),
};

extern const EntSpec kGoldNuggetSpec{
    .type_ = EntType::GoldNugget,
    .size = Vec2::New(8.0F, 8.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .buoyancy = 0.35F,
    .self_light = 0.18F,
    .light_strength = 0.22F,
    .light_color = kGoldLightColor,
    .light_radius = 3,
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::CrushingOnly,
    .proj_contact_damage_amount = 0,
    .can_apply_proj_contact = false,
    .on_ent_contact = OnEntContactAsGoldNugget,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::GoldNugget),
};

extern const EntSpec kGoldBarSpec{
    .type_ = EntType::GoldBar,
    .size = Vec2::New(8.0F, 8.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .buoyancy = 0.35F,
    .self_light = 0.18F,
    .light_strength = 0.22F,
    .light_color = kGoldLightColor,
    .light_radius = 3,
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::CrushingOnly,
    .proj_contact_damage_amount = 0,
    .can_apply_proj_contact = false,
    .on_ent_contact = OnEntContactAsGoldBar,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::GoldBar),
};

extern const EntSpec kGoldBarsSpec{
    .type_ = EntType::GoldBars,
    .size = Vec2::New(16.0F, 16.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .buoyancy = 0.25F,
    .self_light = 0.22F,
    .light_strength = 0.28F,
    .light_color = kGoldLightColor,
    .light_radius = 4,
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::CrushingOnly,
    .proj_contact_damage_amount = 0,
    .can_apply_proj_contact = false,
    .on_ent_contact = OnEntContactAsGoldBars,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::GoldBars),
};

} // namespace splonks::ents::money

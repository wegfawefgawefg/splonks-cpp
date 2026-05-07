#include "entities/money.hpp"

#include "audio_emitters.hpp"
#include "entities/common/common.hpp"
#include "effects/treasure_pickup.hpp"

#include "entity/archetype.hpp"
#include "entity/core_types.hpp"
#include "frame_data_animator.hpp"
#include "frame_data_id.hpp"
#include "gameplay_events.hpp"
#include "math_types.hpp"

namespace splonks::entities::money {

namespace {

constexpr Color3 kGoldLightColor = Color3::New(1.0F, 0.78F, 0.24F);

common::ContactResolution TryCollectMoneyPickup(
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
        return common::ContactResolution{};
    }

    Entity& collector = state.entity_manager.entities[collector_idx];
    const Entity& pickup = state.entity_manager.entities[pickup_idx];
    collector.money += amount;
    EmitPlayerStatePatchedGameplayEvent(state, collector);
    (void)PlayEntityCenterSoundEmitter(state, pickup, sound);
    effects::SpawnTreasurePickupSparkles(pickup, state, kGoldLightColor, sparkle_count);
    common::DeactivateCollectedPickup(pickup_idx, state, *graphics);
    return common::ContactResolution{};
}

common::ContactResolution OnEntityContactAsGold(
    std::size_t entity_idx,
    std::size_t other_entity_idx,
    const common::ContactContext&,
    State& state,
    const Graphics* graphics,
    Audio* audio
) {
    return TryCollectMoneyPickup(entity_idx, other_entity_idx, state, graphics, audio, 500, audio_asset_ids::Gold, 2);
}

common::ContactResolution OnEntityContactAsGoldStack(
    std::size_t entity_idx,
    std::size_t other_entity_idx,
    const common::ContactContext&,
    State& state,
    const Graphics* graphics,
    Audio* audio
) {
    return TryCollectMoneyPickup(entity_idx, other_entity_idx, state, graphics, audio, 1500, audio_asset_ids::GoldStack, 3);
}

common::ContactResolution OnEntityContactAsGoldChunk(
    std::size_t entity_idx,
    std::size_t other_entity_idx,
    const common::ContactContext&,
    State& state,
    const Graphics* graphics,
    Audio* audio
) {
    return TryCollectMoneyPickup(entity_idx, other_entity_idx, state, graphics, audio, 100, audio_asset_ids::Gold, 3);
}

common::ContactResolution OnEntityContactAsGoldNugget(
    std::size_t entity_idx,
    std::size_t other_entity_idx,
    const common::ContactContext&,
    State& state,
    const Graphics* graphics,
    Audio* audio
) {
    return TryCollectMoneyPickup(entity_idx, other_entity_idx, state, graphics, audio, 500, audio_asset_ids::GoldStack, 5);
}

common::ContactResolution OnEntityContactAsGoldBar(
    std::size_t entity_idx,
    std::size_t other_entity_idx,
    const common::ContactContext&,
    State& state,
    const Graphics* graphics,
    Audio* audio
) {
    return TryCollectMoneyPickup(entity_idx, other_entity_idx, state, graphics, audio, 500, audio_asset_ids::GoldStack, 5);
}

common::ContactResolution OnEntityContactAsGoldBars(
    std::size_t entity_idx,
    std::size_t other_entity_idx,
    const common::ContactContext&,
    State& state,
    const Graphics* graphics,
    Audio* audio
) {
    return TryCollectMoneyPickup(entity_idx, other_entity_idx, state, graphics, audio, 1500, audio_asset_ids::GoldStack, 7);
}

} // namespace

extern const EntityArchetype kGoldArchetype{
    .type_ = EntityType::Gold,
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
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::CrushingOnly,
    .projectile_contact_damage_amount = 0,
    .can_apply_projectile_contact = false,
    .on_entity_contact = OnEntityContactAsGold,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::GoldCoin),
};

extern const EntityArchetype kGoldStackArchetype{
    .type_ = EntityType::GoldStack,
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
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::CrushingOnly,
    .projectile_contact_damage_amount = 0,
    .can_apply_projectile_contact = false,
    .on_entity_contact = OnEntityContactAsGoldStack,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::GoldStack),
};

extern const EntityArchetype kGoldChunkArchetype{
    .type_ = EntityType::GoldChunk,
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
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::CrushingOnly,
    .projectile_contact_damage_amount = 0,
    .can_apply_projectile_contact = false,
    .on_entity_contact = OnEntityContactAsGoldChunk,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::GoldChunk),
};

extern const EntityArchetype kGoldNuggetArchetype{
    .type_ = EntityType::GoldNugget,
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
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::CrushingOnly,
    .projectile_contact_damage_amount = 0,
    .can_apply_projectile_contact = false,
    .on_entity_contact = OnEntityContactAsGoldNugget,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::GoldNugget),
};

extern const EntityArchetype kGoldBarArchetype{
    .type_ = EntityType::GoldBar,
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
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::CrushingOnly,
    .projectile_contact_damage_amount = 0,
    .can_apply_projectile_contact = false,
    .on_entity_contact = OnEntityContactAsGoldBar,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::GoldBar),
};

extern const EntityArchetype kGoldBarsArchetype{
    .type_ = EntityType::GoldBars,
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
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::CrushingOnly,
    .projectile_contact_damage_amount = 0,
    .can_apply_projectile_contact = false,
    .on_entity_contact = OnEntityContactAsGoldBars,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::GoldBars),
};

} // namespace splonks::entities::money

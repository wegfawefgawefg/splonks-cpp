#include "entities/emerald_big.hpp"

#include "audio_emitters.hpp"
#include "effects/treasure_pickup.hpp"
#include "entities/common/common.hpp"

#include "entity/archetype.hpp"
#include "entity/core_types.hpp"
#include "frame_data_animator.hpp"
#include "frame_data_id.hpp"
#include "gameplay_events.hpp"
#include "math_types.hpp"

namespace splonks::entities::emerald_big {

namespace {

common::ContactResolution OnEntityContactAsEmeraldBig(
    std::size_t entity_idx,
    std::size_t other_entity_idx,
    const common::ContactContext&,
    State& state,
    const Graphics* graphics,
    Audio* audio
) {
    if (graphics == nullptr || audio == nullptr ||
        !common::CanCollectPickupFromContact(entity_idx, other_entity_idx, state)) {
        return common::ContactResolution{};
    }
    if (common::TryRequestCollectPickupFromContact(entity_idx, other_entity_idx, state)) {
        return common::ContactResolution{};
    }

    Entity& collector = state.entity_manager.entities[other_entity_idx];
    const Entity& gem = state.entity_manager.entities[entity_idx];
    collector.money += 800;
    EmitPlayerStatePatchedGameplayEvent(state, collector);
    (void)PlayEntityCenterSoundEmitter(state, gem, audio_asset_ids::GoldStack);
    effects::SpawnTreasurePickupSparkles(gem, state, Color3::New(0.18F, 1.0F, 0.38F), 6);
    common::DeactivateCollectedPickup(entity_idx, state, *graphics);
    return common::ContactResolution{};
}

} // namespace

extern const EntityArchetype kEmeraldBigArchetype{
    .type_ = EntityType::EmeraldBig,
    .size = Vec2::New(16.0F, 16.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = false,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .self_light = 0.28F,
    .light_strength = 0.45F,
    .light_color = Color3::New(0.18F, 1.0F, 0.38F),
    .light_radius = 5,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Immune,
    .projectile_contact_damage_amount = 0,
    .can_apply_projectile_contact = false,
    .on_entity_contact = OnEntityContactAsEmeraldBig,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::EmeraldBig),
};

} // namespace splonks::entities::emerald_big

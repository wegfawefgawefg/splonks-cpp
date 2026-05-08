#include "entities/gear_items.hpp"

#include "audio_emitters.hpp"
#include "entity/archetype.hpp"
#include "frame_data_id.hpp"
#include "state.hpp"
#include "world_ops.hpp"

#include <algorithm>
#include <cstdint>

namespace splonks::entities::gear_items {

namespace {

constexpr float kParachuteMaxFallSpeed = 1.35F;
constexpr float kParachuteVisualOffsetY = -12.0F;
constexpr float kCapeMaxFallSpeed = 1.35F;

common::ContactResolution OnEntityContactAsInventoryPickup(
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
    const Entity& pickup = state.entity_manager.entities[entity_idx];
    if (!TryCollectInventoryPickup(state, collector, pickup)) {
        return common::ContactResolution{};
    }

    (void)PlayEntityCenterSoundEmitter(state, pickup, audio_asset_ids::Equip);
    if (pickup.type_ == EntityType::SpringShoes) {
        (void)PlayEntityCenterSoundEmitter(state, pickup, audio_asset_ids::SpringShoe);
    }
    common::DeactivateCollectedPickup(entity_idx, state, *graphics);
    return common::ContactResolution{};
}

Entity* GetOpenParachuteVisual(Entity& owner, State& state) {
    if (!owner.entity_b.has_value()) {
        return nullptr;
    }
    Entity* const parachute = state.entity_manager.GetEntityMut(*owner.entity_b);
    if (parachute == nullptr || !parachute->active || parachute->type_ != EntityType::Parachute ||
        parachute->frame_data_animator.animation_id != frame_data_ids::OpenParachute) {
        owner.entity_b.reset();
        return nullptr;
    }
    return parachute;
}

std::uint32_t GetParachuteDeployFallFrames(const State& state) {
    return static_cast<std::uint32_t>(std::max(0, state.player_tuning.fall_damage_light_frames));
}

void ClearOpenParachuteVisual(Entity& owner, State& state, const Graphics& graphics) {
    Entity* const parachute = GetOpenParachuteVisual(owner, state);
    if (parachute == nullptr) {
        return;
    }
    if (!world_ops::DeactivateEntity(state, parachute->vid)) {
        return;
    }
    owner.entity_b.reset();
    state.UpdateSidForEntity(parachute->vid.id, graphics);
}

void UpdateOpenParachuteVisual(Entity& owner, State& state, const Graphics& graphics) {
    Entity* parachute = GetOpenParachuteVisual(owner, state);
    bool spawned_visual = false;
    if (parachute == nullptr) {
        parachute = world_ops::SpawnEntity(
            state,
            EntityType::Parachute,
            [](Entity& spawned) {
                SetAnimation(spawned, frame_data_ids::OpenParachute);
                spawned.has_physics = false;
                spawned.can_collide = false;
                spawned.can_be_hit = false;
                spawned.can_be_picked_up = false;
                spawned.draw_layer = DrawLayer::Background;
            }
        );
        if (parachute == nullptr) {
            return;
        }
        owner.entity_b = parachute->vid;
        spawned_visual = true;
    }

    const Vec2 owner_visual_center =
        common::GetVisualCenterForEntity(owner, graphics, owner.GetCenter());
    parachute->SetCenter(owner_visual_center + Vec2::New(0.0F, kParachuteVisualOffsetY));
    parachute->vel = Vec2::New(0.0F, 0.0F);
    parachute->acc = Vec2::New(0.0F, 0.0F);
    state.UpdateSidForEntity(parachute->vid.id, graphics);
    world_ops::PatchEntityState(state, *parachute, *parachute);
    if (spawned_visual) {
        world_ops::PatchEntityState(state, owner, owner);
    }
}

void StepEquippedParachute(Entity& owner, State& state, const Graphics& graphics) {
    if (owner.grounded || owner.condition != EntityCondition::Normal) {
        ClearOpenParachuteVisual(owner, state, graphics);
        return;
    }

    const bool already_open = GetOpenParachuteVisual(owner, state) != nullptr;
    if (!already_open) {
        if (!HasEffect(owner, EffectId::Parachute) ||
            owner.fall_timer < GetParachuteDeployFallFrames(state)) {
            return;
        }
        RemoveEffect(owner, EffectId::Parachute);
    }

    owner.vel.y = std::min(owner.vel.y, kParachuteMaxFallSpeed);
    owner.fall_timer = 0;
    UpdateOpenParachuteVisual(owner, state, graphics);
}

FrameDataId GetCapeAnimation(const Entity& cape, const State& state) {
    const bool open = cape.counter_a > 0.0F;
    if (cape.attachment_mode == AttachmentMode::Back && cape.held_by_vid.has_value()) {
        const Entity* const holder = state.entity_manager.GetEntity(*cape.held_by_vid);
        if (holder != nullptr) {
            if (holder->IsHanging()) {
                return open ? frame_data_ids::CapeSideOpen : frame_data_ids::CapeSide;
            }
            if (holder->IsClimbing()) {
                return open ? frame_data_ids::CapeBackOpen : frame_data_ids::CapeBack;
            }
        }
        return open ? frame_data_ids::CapeOpen : frame_data_ids::Cape;
    }

    if (cape.attachment_mode == AttachmentMode::Held || cape.held_by_vid.has_value()) {
        return open ? frame_data_ids::CapeSideOpen : frame_data_ids::CapeSide;
    }
    return frame_data_ids::CapeClosed;
}

void OnUseAsCape(std::size_t entity_idx, State& state, Graphics& graphics, Audio& audio) {
    (void)graphics;
    (void)audio;
    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    Entity& cape = state.entity_manager.entities[entity_idx];
    cape.counter_a = 0.0F;
    if (!cape.use_state.down ||
        !cape.use_state.user_vid.has_value()) {
        return;
    }
    if (cape.use_state.source != AttachmentMode::Back &&
        cape.use_state.source != AttachmentMode::Held) {
        return;
    }

    Entity* const holder = state.entity_manager.GetEntityMut(*cape.use_state.user_vid);
    if (holder == nullptr || holder->condition != EntityCondition::Normal) {
        return;
    }

    cape.counter_a = 1.0F;
    if (!holder->grounded && holder->vel.y > 0.0F) {
        holder->vel.y = std::min(holder->vel.y, kCapeMaxFallSpeed);
        holder->fall_timer = 0;
    }
}

void StepEntityLogicAsCape(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    (void)dt;
    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    Entity& cape = state.entity_manager.entities[entity_idx];
    if (!cape.use_state.down) {
        cape.counter_a = 0.0F;
    }
    SetAnimation(cape, GetCapeAnimation(cape, state));
}

} // namespace

void StepEquippedPassiveItems(std::size_t entity_idx, State& state, Graphics& graphics) {
    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    Entity& entity = state.entity_manager.entities[entity_idx];
    StepEquippedParachute(entity, state, graphics);
}

void ClearEquippedPassiveItemVisuals(Entity& entity, State& state, const Graphics& graphics) {
    ClearOpenParachuteVisual(entity, state, graphics);
}

extern const EntityArchetype kCapeArchetype{
    .type_ = EntityType::Cape,
    .size = Vec2::New(16.0F, 16.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_go_on_back = true,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Vulnerable,
    .on_use = OnUseAsCape,
    .step_logic = StepEntityLogicAsCape,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::CapePickup),
};
extern const EntityArchetype kGlovesArchetype{
    .type_ = EntityType::Gloves,
    .size = Vec2::New(16.0F, 16.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Vulnerable,
    .pickup_effect = EffectId::Gloves,
    .on_entity_contact = OnEntityContactAsInventoryPickup,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::Gloves),
};
extern const EntityArchetype kSpectaclesArchetype{
    .type_ = EntityType::Spectacles,
    .size = Vec2::New(16.0F, 16.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Vulnerable,
    .pickup_effect = EffectId::Spectacles,
    .on_entity_contact = OnEntityContactAsInventoryPickup,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::Spectacles),
};
extern const EntityArchetype kMittArchetype{
    .type_ = EntityType::Mitt,
    .size = Vec2::New(16.0F, 16.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Vulnerable,
    .pickup_effect = EffectId::Mitt,
    .on_entity_contact = OnEntityContactAsInventoryPickup,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::Mitt),
};
extern const EntityArchetype kPasteArchetype{
    .type_ = EntityType::Paste,
    .size = Vec2::New(16.0F, 16.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Vulnerable,
    .on_entity_contact = OnEntityContactAsInventoryPickup,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::SpiderMilk),
};
extern const EntityArchetype kSpringShoesArchetype{
    .type_ = EntityType::SpringShoes,
    .size = Vec2::New(16.0F, 16.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Vulnerable,
    .pickup_effect = EffectId::SpringShoes,
    .on_entity_contact = OnEntityContactAsInventoryPickup,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::SpringShoes),
};
extern const EntityArchetype kSpikeShoesArchetype{
    .type_ = EntityType::SpikeShoes,
    .size = Vec2::New(16.0F, 16.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Vulnerable,
    .pickup_effect = EffectId::SpikeShoes,
    .on_entity_contact = OnEntityContactAsInventoryPickup,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::SpikeShoes),
};
extern const EntityArchetype kBombBoxArchetype{
    .type_ = EntityType::BombBox,
    .size = Vec2::New(16.0F, 16.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = false,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Vulnerable,
    .on_entity_contact = OnEntityContactAsInventoryPickup,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::BombBox),
};
extern const EntityArchetype kBombBagArchetype{
    .type_ = EntityType::BombBag,
    .size = Vec2::New(16.0F, 16.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = false,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Vulnerable,
    .on_entity_contact = OnEntityContactAsInventoryPickup,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::BombBag),
};
extern const EntityArchetype kCompassArchetype{
    .type_ = EntityType::Compass,
    .size = Vec2::New(16.0F, 16.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Vulnerable,
    .pickup_effect = EffectId::Compass,
    .on_entity_contact = OnEntityContactAsInventoryPickup,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::Compass),
};
extern const EntityArchetype kParachuteArchetype{
    .type_ = EntityType::Parachute,
    .size = Vec2::New(16.0F, 16.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Vulnerable,
    .pickup_effect = EffectId::Parachute,
    .on_entity_contact = OnEntityContactAsInventoryPickup,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::PackedParachute),
};
extern const EntityArchetype kRopePileArchetype{
    .type_ = EntityType::RopePile,
    .size = Vec2::New(16.0F, 16.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = false,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Vulnerable,
    .on_entity_contact = OnEntityContactAsInventoryPickup,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::RopePile),
};

} // namespace splonks::entities::gear_items

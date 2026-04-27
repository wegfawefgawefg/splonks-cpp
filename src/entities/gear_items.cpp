#include "entities/gear_items.hpp"

#include "audio_emitters.hpp"
#include "entity/archetype.hpp"
#include "frame_data_id.hpp"
#include "world_query.hpp"

#include <algorithm>
#include <cstdint>

namespace splonks::entities::gear_items {

namespace {

constexpr std::uint32_t kParachuteDeployFallFrames = 28;
constexpr float kParachuteMaxFallSpeed = 1.35F;
constexpr float kParachuteVisualOffsetY = -12.0F;

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

    Entity& collector = state.entity_manager.entities[other_entity_idx];
    const Entity& pickup = state.entity_manager.entities[entity_idx];
    if (!TryCollectInventoryPickup(state, collector, pickup)) {
        return common::ContactResolution{};
    }

    (void)PlayEntityCenterSoundEmitter(state, pickup, audio_asset_ids::Equip);
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

bool HasSolidParachuteBlockerBelow(const Entity& owner, const State& state) {
    const IVec2 probe = ToIVec2(owner.GetCenter() + Vec2::New(0.0F, 32.0F));
    const std::optional<WorldTileQueryResult> tile_query =
        QueryTileAtWorldPos(state.stage, probe);
    return tile_query.has_value() && tile_query->tile != nullptr &&
           IsTileCollidable(*tile_query->tile);
}

void ClearOpenParachuteVisual(Entity& owner, State& state, const Graphics& graphics) {
    Entity* const parachute = GetOpenParachuteVisual(owner, state);
    if (parachute == nullptr) {
        return;
    }
    state.entity_manager.SetInactive(parachute->vid.id);
    state.UpdateSidForEntity(parachute->vid.id, graphics);
    owner.entity_b.reset();
}

void UpdateOpenParachuteVisual(Entity& owner, State& state, const Graphics& graphics) {
    Entity* parachute = GetOpenParachuteVisual(owner, state);
    if (parachute == nullptr) {
        const std::optional<VID> vid = state.entity_manager.NewEntity();
        if (!vid.has_value()) {
            return;
        }
        parachute = state.entity_manager.GetEntityMut(*vid);
        if (parachute == nullptr) {
            return;
        }
        SetEntityAs(*parachute, EntityType::Parachute);
        SetAnimation(*parachute, frame_data_ids::OpenParachute);
        parachute->has_physics = false;
        parachute->can_collide = false;
        parachute->can_be_hit = false;
        parachute->can_be_picked_up = false;
        parachute->draw_layer = DrawLayer::Background;
        owner.entity_b = *vid;
    }

    const Vec2 owner_visual_center =
        common::GetVisualCenterForEntity(owner, graphics, owner.GetCenter());
    parachute->SetCenter(owner_visual_center + Vec2::New(0.0F, kParachuteVisualOffsetY));
    parachute->vel = Vec2::New(0.0F, 0.0F);
    parachute->acc = Vec2::New(0.0F, 0.0F);
    state.UpdateSidForEntity(parachute->vid.id, graphics);
}

void StepEquippedParachute(Entity& owner, State& state, const Graphics& graphics) {
    if (owner.grounded || owner.IsClimbing() || owner.IsHanging() ||
        owner.condition != EntityCondition::Normal) {
        ClearOpenParachuteVisual(owner, state, graphics);
        return;
    }

    const bool already_open = GetOpenParachuteVisual(owner, state) != nullptr;
    if (!already_open) {
        if (!HasPassiveItem(owner, EntityPassiveItem::Parachute) ||
            owner.fall_timer <= kParachuteDeployFallFrames ||
            HasSolidParachuteBlockerBelow(owner, state)) {
            return;
        }
        SetPassiveItem(owner, EntityPassiveItem::Parachute, false);
    }

    owner.vel.y = std::min(owner.vel.y, kParachuteMaxFallSpeed);
    owner.fall_timer = 0;
    UpdateOpenParachuteVisual(owner, state, graphics);
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
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Vulnerable,
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
    .passive_item = EntityPassiveItem::Gloves,
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
    .passive_item = EntityPassiveItem::Spectacles,
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
    .passive_item = EntityPassiveItem::Mitt,
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
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::Paste),
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
    .passive_item = EntityPassiveItem::SpringShoes,
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
    .passive_item = EntityPassiveItem::SpikeShoes,
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
    .passive_item = EntityPassiveItem::Compass,
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
    .passive_item = EntityPassiveItem::Parachute,
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

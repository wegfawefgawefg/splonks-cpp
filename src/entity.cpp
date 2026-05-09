#include "entity.hpp"

#include "entity/display_states.hpp"
#include "entity/display_support.hpp"
#include "entity/archetype_restore.hpp"
#include "frame_data_id.hpp"
#include "tile.hpp"
#include "world_ops.hpp"
#include "world_query.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace splonks {

namespace {

constexpr std::uint32_t MovementFlagBit(EntityMovementFlag movement_flag) {
    return 1U << static_cast<unsigned int>(movement_flag);
}

constexpr float kGroundProbeFractionalEpsilon = 0.125F;

} // namespace

Entity Entity::New() {
    Entity entity;
    entity.active = false;
    entity.marked_for_destruction = false;
    entity.type_ = EntityType::None;
    entity.vid = VID{0, 0};
    entity.has_physics = true;
    entity.can_collide = true;
    entity.can_be_hit = true;
    entity.can_receive_projectile_contact = true;
    entity.stone = false;
    entity.crusher_pusher = false;
    entity.pushable = false;
    entity.can_stomp = false;
    entity.can_be_stomped = true;
    entity.can_collect_pickups = false;
    entity.grounded = false;
    entity.shake = 0.0F;
    entity.rotation = 0.0F;
    entity.alpha = 1.0F;
    entity.coyote_time = 0;
    entity.stun_timer = 0;
    entity.stun_recovers_on_ground = true;
    entity.stun_recovers_while_held = true;
    entity.can_be_picked_up = true;
    entity.affected_by_cobweb = true;
    entity.can_only_be_picked_up_if_dead_or_stunned = false;
    entity.impassable = false;
    entity.can_be_hung_on = true;
    entity.fall_timer = 0;
    entity.pos = Vec2::New(0.0F, 0.0F);
    entity.vel = Vec2::New(0.0F, 0.0F);
    entity.acc = Vec2::New(0.0F, 0.0F);
    entity.max_speed = 7.0F;
    entity.jump_hold_gravity_frames_remaining = 0;
    entity.throw_velocity_scale = 1.0F;
    entity.buoyancy = 0.0F;
    entity.size = Vec2::New(8.0F, 8.0F);
    entity.self_light = 0.0F;
    entity.light_strength = 0.0F;
    entity.light_color = Color3::White();
    entity.light_radius = 0;
    entity.dist_traveled_this_frame = 0.0F;
    entity.facing = LeftOrRight::Left;
    entity.vertical_flip = false;
    entity.draw_layer = DrawLayer::Middle;
    entity.render_enabled = true;
    TrySetAnimation(entity, EntityDisplayState::Neutral);
    entity.frame_data_animator = FrameDataAnimator{};
    entity.jump_delay_frame_count = kJumpDelayFrames;
    entity.jumped_this_frame = false;
    entity.climb_detach_cooldown = 0;
    entity.hang_side.reset();
    entity.can_hang_ledge = false;
    entity.can_hang_wall = false;
    entity.hang_count = 0;
    entity.holding = false;
    entity.effects.reset();
    entity.pickup_effect.reset();
    entity.money = 0;
    entity.buyable = Buyable{};
    entity.stage_spawn_index.reset();
    entity.attachment_mode = AttachmentMode::None;
    entity.use_state = UseState{};
    entity.travel_sound_countdown = kTravelSoundDistInterval;
    entity.travel_sound = TravelSound::One;
    entity.condition = EntityCondition::Normal;
    entity.last_condition = EntityCondition::Normal;
    entity.ai_state = EntityAiState::Idle;
    entity.last_ai_state = EntityAiState::Idle;
    entity.movement_flags = 0;
    entity.health = 0;
    entity.hurt_on_contact = false;
    entity.vanish_on_death = false;
    entity.affected_by_ground_friction = true;
    entity.support_ground_friction = 0.85F;
    entity.push_acc = 0.0F;
    entity.damage_animation.reset();
    entity.damage_sound.reset();
    entity.collide_sound.reset();
    entity.death_sound.reset();
    entity.on_death = nullptr;
    entity.on_damage = nullptr;
    entity.on_use = nullptr;
    entity.on_area_enter = nullptr;
    entity.on_area_exit = nullptr;
    entity.on_area_tile_changed = nullptr;
    entity.control_logic = nullptr;
    entity.step_logic = nullptr;
    entity.step_physics = nullptr;
    entity.transition_target.reset();
    entity.stage_exit_id = kInvalidStageExitId;
    entity.damage_vulnerability = DamageVulnerability::Vulnerable;
    entity.attack_weight = 0.0F;
    entity.weight = 0.0F;
    entity.bomb_throw_delay_countdown = 0;
    entity.rope_throw_delay_countdown = 0;
    entity.attack_delay_countdown = 0;
    entity.equip_delay_countdown = 0;
    entity.thrown_immunity_timer = 0;
    entity.projectile_contact_damage_type = DamageType::Attack;
    entity.projectile_contact_damage_amount = 1;
    entity.can_apply_projectile_contact = true;
    entity.projectile_contact_timer = 0;
    entity.collided = false;
    entity.collided_last_frame = false;
    entity.contact_sound_cooldown = 0;
    entity.can_be_stunned = false;
    entity.point_a = IVec2::New(0, 0);
    entity.point_b = IVec2::New(0, 0);
    entity.point_c = IVec2::New(0, 0);
    entity.point_d = IVec2::New(0, 0);
    entity.point_label_a = PointLabel::None;
    entity.point_label_b = PointLabel::None;
    entity.point_label_c = PointLabel::None;
    entity.point_label_d = PointLabel::None;
    entity.holding_timer = kDefaultHoldingTimer;
    entity.entity_label_a = EntityLabel::None;
    entity.child_vids.reset();
    entity.inside_vids.reset();
    entity.alignment = Alignment::Neutral;
    entity.counter_a = 0.0F;
    entity.counter_b = 0.0F;
    entity.counter_c = 0.0F;
    entity.counter_d = 0.0F;
    entity.threshold_a = 0.0F;
    entity.threshold_b = 0.0F;
    return entity;
}

void Entity::Reset() {
    const VID existing_vid = vid;
    *this = Entity::New();
    vid = existing_vid;
    active = true;
}

void AddEntityShake(Entity& entity, float amount) {
    constexpr float kMaxEntityShake = 8.0F;
    entity.shake = std::clamp(entity.shake + amount, 0.0F, kMaxEntityShake);
}

void AttenuateEntityShake(Entity& entity, float amount) {
    entity.shake = std::max(0.0F, entity.shake - amount);
}

void UseEntity(Entity& entity, std::optional<VID> user_vid, AttachmentMode source) {
    const bool was_down = entity.use_state.down;
    entity.use_state.down = true;
    entity.use_state.pressed = !was_down;
    entity.use_state.released = false;
    entity.use_state.frames = was_down ? entity.use_state.frames + 1 : 1;
    entity.use_state.user_vid = user_vid;
    entity.use_state.source = source;
}

void PressUseEntity(Entity& entity, std::optional<VID> user_vid, AttachmentMode source) {
    UseEntity(entity, user_vid, source);
    entity.use_state.pressed = true;
}

void ReleaseUseEntity(Entity& entity, std::optional<VID> user_vid, AttachmentMode source) {
    entity.use_state.down = false;
    entity.use_state.pressed = false;
    entity.use_state.released = true;
    entity.use_state.frames = 0;
    entity.use_state.user_vid = user_vid;
    entity.use_state.source = source;
}

void StopUsingEntity(Entity& entity) {
    const bool was_down = entity.use_state.down;
    entity.use_state.down = false;
    entity.use_state.pressed = false;
    entity.use_state.released = was_down;
    entity.use_state.frames = 0;
    entity.use_state.user_vid.reset();
    entity.use_state.source = AttachmentMode::None;
}

bool HasMovementFlag(const Entity& entity, EntityMovementFlag movement_flag) {
    return (entity.movement_flags & MovementFlagBit(movement_flag)) != 0;
}

void SetMovementFlag(Entity& entity, EntityMovementFlag movement_flag, bool enabled) {
    if (enabled) {
        entity.movement_flags |= MovementFlagBit(movement_flag);
        return;
    }

    entity.movement_flags &= ~MovementFlagBit(movement_flag);
}

void ClearTransientMovementFlags(Entity& entity) {
    SetMovementFlag(entity, EntityMovementFlag::Walking, false);
    SetMovementFlag(entity, EntityMovementFlag::Running, false);
    SetMovementFlag(entity, EntityMovementFlag::Pushing, false);
}

std::tuple<Vec2, Vec2> Entity::GetBounds() const {
    return {pos, pos + size - Vec2::New(1.0F, 1.0F)};
}

AABB Entity::GetAABB() const {
    return AABB::New(pos, pos + size - Vec2::New(1.0F, 1.0F));
}

Vec2 Entity::GetCenter() const {
    return pos + size / 2.0F;
}

void Entity::SetCenter(const Vec2& center) {
    pos = center - size / 2.0F;
}

void Entity::IncTravelSound() {
    switch (travel_sound) {
    case TravelSound::One:
        travel_sound = TravelSound::Two;
        return;
    case TravelSound::Two:
        travel_sound = TravelSound::One;
        return;
    }
}

bool Entity::IsHanging() const {
    return hang_side.has_value();
}

bool Entity::IsClimbing() const {
    return HasMovementFlag(*this, EntityMovementFlag::Climbing);
}

AABB Entity::GetFeet() const {
    const auto [tl, br] = GetBounds();
    return AABB::New(Vec2::New(tl.x, br.y), br + Vec2::New(0.0F, 1.0F));
}

AABB Entity::GetGroundProbe() const {
    AABB feet = GetFeet();
    feet.br.y += kGroundProbeFractionalEpsilon;
    return feet;
}

bool Entity::TrySnapToBlockingStageBottom(const Stage& stage) {
    if (!stage.IsBorderSideBlocking(StageBorderSideKind::Bottom)) {
        return false;
    }

    const AABB ground_probe = GetGroundProbe();
    if (ground_probe.br.y < static_cast<float>(stage.GetHeight())) {
        return false;
    }

    pos.y = std::round(static_cast<float>(stage.GetHeight()) - size.y);
    return true;
}

void Entity::SetGrounded(const Stage& stage) {
    const AABB feet = GetGroundProbe();
    if (TrySnapToBlockingStageBottom(stage)) {
        grounded |= true;
        return;
    }

    for (const WorldTileQueryResult& tile_query : QueryTilesInAabb(stage, feet)) {
        if (tile_query.tile != nullptr &&
            (IsTileCollidable(*tile_query.tile) ||
             (!IsClimbing() && IsOneWayTopTileSupportingAabb(stage, tile_query, feet)))) {
            grounded = true;
            return;
        }
    }
}

std::tuple<Vec2, Vec2> Entity::GetTlAndTrCorners() const {
    return {Vec2::New(pos.x, pos.y), Vec2::New(pos.x + size.x, pos.y)};
}

HangHands Entity::GetHangHands() const {
    const auto [tl, tr] = GetTlAndTrCorners();
    HangHands hang_hands;
    hang_hands.left = tl;
    hang_hands.right = tr;
    return hang_hands;
}

HangHandBounds Entity::GetHangHandsBounds() const {
    const auto [tl, _br] = GetBounds();
    const Vec2 right_edge = tl + Vec2::New(size.x, 0.0F);
    HangHandBounds hang_hands;
    hang_hands.left_tl = tl - kHangHandSize;
    hang_hands.left_br = tl;
    hang_hands.right_tl = right_edge - Vec2::New(0.0F, kHangHandSize.y);
    hang_hands.right_br = right_edge + Vec2::New(kHangHandSize.x, 0.0F);
    return hang_hands;
}

bool TrySetAnimation(Entity& entity, EntityDisplayState display_state) {
    const auto selection = GetFrameDataSelectionForDisplayState(EntityDisplayInput{
        .type_ = entity.type_,
        .display_state = display_state,
    });
    if (!selection.has_value()) {
        return false;
    }

    SetAnimation(entity, selection->animation_id);
    entity.frame_data_animator.animate = selection->animate;
    if (selection->has_forced_frame) {
        entity.frame_data_animator.SetForcedFrame(selection->forced_frame);
    }
    return true;
}

void SetAnimation(Entity& entity, FrameDataId animation_id) {
    entity.frame_data_animator.SetAnimation(animation_id);
}

bool TryCollectEffectPickup(Entity& entity, const Entity& pickup) {
    if (!pickup.pickup_effect.has_value()) {
        return false;
    }

    (void)AddEffect(entity, *pickup.pickup_effect, GetEffectArchetype(*pickup.pickup_effect).default_count);
    return true;
}

bool TryCollectInventoryPickup(State& state, Entity& entity, const Entity& pickup) {
    bool collected = false;
    switch (pickup.type_) {
    case EntityType::BombBox:
        collected |= state.entity_tools.AddToolCount(entity.vid, ToolKind::ThrowBomb, 12);
        break;
    case EntityType::BombBag:
        collected |= state.entity_tools.AddToolCount(entity.vid, ToolKind::ThrowBomb, 3);
        break;
    case EntityType::Paste:
        collected |= state.entity_tools.UpgradeBombsToSticky(entity.vid);
        break;
    case EntityType::RopePile:
        collected |= state.entity_tools.AddToolCount(entity.vid, ToolKind::ThrowRope, 3);
        break;
    default:
        break;
    }
    if (TryCollectEffectPickup(entity, pickup)) {
        collected = true;
    }
    if (collected) {
        world_ops::PatchPlayerState(state, entity);
    }
    return collected;
}

bool CanRevealEmbeddedTreasure(const Entity& entity) {
    return GetModifiedEffectValue(entity, EffectModifierTarget::HiddenTreasureVisibility, 0.0F) > 0.0F;
}

void EnableStone(Entity& entity) {
    entity.stone = true;
    entity.crusher_pusher = true;
    entity.impassable = true;
    entity.damage_vulnerability = DamageVulnerability::ExplosionOnly;
}

void DisableStone(Entity& entity) {
    entity.stone = false;
    RestoreEntityCrusherPusherFromArchetype(entity);
    RestoreEntityImpassableFromArchetype(entity);
    RestoreEntityDamageVulnerabilityFromArchetype(entity);
}

} // namespace splonks

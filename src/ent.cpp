#include "ent.hpp"

#include "ent/display_states.hpp"
#include "ent/display_support.hpp"
#include "ent/spec_restore.hpp"
#include "aframe_id.hpp"
#include "tile.hpp"
#include "world_ops.hpp"
#include "world_query.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace splonks {

namespace {

constexpr std::uint32_t MovementFlagBit(EntMovementFlag movement_flag) {
    return 1U << static_cast<unsigned int>(movement_flag);
}

constexpr float kGroundProbeFractionalEpsilon = 0.125F;

} // namespace

Ent Ent::New() {
    Ent ent;
    ent.active = false;
    ent.marked_for_destruction = false;
    ent.type_ = EntType::None;
    ent.vid = VID{0, 0};
    ent.has_physics = true;
    ent.can_collide = true;
    ent.can_be_hit = true;
    ent.can_receive_proj_contact = true;
    ent.stone = false;
    ent.crusher_pusher = false;
    ent.pushable = false;
    ent.can_stomp = false;
    ent.can_be_stomped = true;
    ent.can_collect_pickups = false;
    ent.grounded = false;
    ent.shake = 0.0F;
    ent.rotation = 0.0F;
    ent.alpha = 1.0F;
    ent.coyote_time = 0;
    ent.stun_timer = 0;
    ent.stun_recovers_on_ground = true;
    ent.stun_recovers_while_held = true;
    ent.can_be_picked_up = true;
    ent.affected_by_cobweb = true;
    ent.can_only_be_picked_up_if_dead_or_stunned = false;
    ent.impassable = false;
    ent.can_be_hung_on = true;
    ent.fall_timer = 0;
    ent.pos = Vec2::New(0.0F, 0.0F);
    ent.vel = Vec2::New(0.0F, 0.0F);
    ent.acc = Vec2::New(0.0F, 0.0F);
    ent.max_speed = 7.0F;
    ent.jump_hold_gravity_frames_remaining = 0;
    ent.throw_velocity_scale = 1.0F;
    ent.buoyancy = 0.0F;
    ent.size = Vec2::New(8.0F, 8.0F);
    ent.self_light = 0.0F;
    ent.light_strength = 0.0F;
    ent.light_color = Color3::White();
    ent.light_radius = 0;
    ent.dist_traveled_this_frame = 0.0F;
    ent.facing = Side::Left;
    ent.vertical_flip = false;
    ent.draw_layer = DrawLayer::Middle;
    ent.render_enabled = true;
    TrySetAnim(ent, EntDisplayState::Neutral);
    ent.aframe_animator = AFrameAnimator{};
    ent.jump_delay_frame_count = kJumpDelayFrames;
    ent.jumped_this_frame = false;
    ent.climb_detach_cooldown = 0;
    ent.hang_side.reset();
    ent.can_hang_ledge = false;
    ent.can_hang_wall = false;
    ent.hang_count = 0;
    ent.holding = false;
    ent.effects.reset();
    ent.pickup_effect.reset();
    ent.money = 0;
    ent.buyable = Buyable{};
    ent.stage_spawn_index.reset();
    ent.attach_mode = AttachMode::None;
    ent.use_state = UseState{};
    ent.travel_sound_countdown = kTravelSoundDistInterval;
    ent.travel_sound = TravelSound::One;
    ent.condition = EntCondition::Normal;
    ent.last_condition = EntCondition::Normal;
    ent.ai_state = EntAiState::Idle;
    ent.last_ai_state = EntAiState::Idle;
    ent.movement_flags = 0;
    ent.health = 0;
    ent.hurt_on_contact = false;
    ent.vanish_on_death = false;
    ent.affected_by_ground_friction = true;
    ent.support_ground_friction = 0.85F;
    ent.push_acc = 0.0F;
    ent.damage_anim.reset();
    ent.damage_sound.reset();
    ent.collide_sound.reset();
    ent.death_sound.reset();
    ent.on_death = nullptr;
    ent.on_damage = nullptr;
    ent.on_use = nullptr;
    ent.on_area_enter = nullptr;
    ent.on_area_exit = nullptr;
    ent.on_area_tile_changed = nullptr;
    ent.control_logic = nullptr;
    ent.step_logic = nullptr;
    ent.step_physics = nullptr;
    ent.transition_target.reset();
    ent.stage_exit_id = kInvalidStageExitId;
    ent.damage_vuln = DamageVuln::Vulnerable;
    ent.attack_weight = 0.0F;
    ent.weight = 0.0F;
    ent.bomb_throw_delay_countdown = 0;
    ent.rope_throw_delay_countdown = 0;
    ent.attack_delay_countdown = 0;
    ent.equip_delay_countdown = 0;
    ent.thrown_immunity_timer = 0;
    ent.proj_contact_damage_type = DamageType::Attack;
    ent.proj_contact_damage_amount = 1;
    ent.can_apply_proj_contact = true;
    ent.proj_contact_timer = 0;
    ent.collided = false;
    ent.collided_last_frame = false;
    ent.contact_sound_cooldown = 0;
    ent.can_be_stunned = false;
    ent.point_a = IVec2::New(0, 0);
    ent.point_b = IVec2::New(0, 0);
    ent.point_c = IVec2::New(0, 0);
    ent.point_d = IVec2::New(0, 0);
    ent.point_label_a = PointLabel::None;
    ent.point_label_b = PointLabel::None;
    ent.point_label_c = PointLabel::None;
    ent.point_label_d = PointLabel::None;
    ent.holding_timer = kDefaultHoldingTimer;
    ent.ent_label_a = EntLabel::None;
    ent.child_vids.reset();
    ent.inside_vids.reset();
    ent.alignment = Alignment::Neutral;
    ent.counter_a = 0.0F;
    ent.counter_b = 0.0F;
    ent.counter_c = 0.0F;
    ent.counter_d = 0.0F;
    ent.threshold_a = 0.0F;
    ent.threshold_b = 0.0F;
    return ent;
}

void Ent::Reset() {
    const VID existing_vid = vid;
    *this = Ent::New();
    vid = existing_vid;
    active = true;
}

void AddEntShake(Ent& ent, float amount) {
    constexpr float kMaxEntShake = 8.0F;
    ent.shake = std::clamp(ent.shake + amount, 0.0F, kMaxEntShake);
}

void AttenuateEntShake(Ent& ent, float amount) {
    ent.shake = std::max(0.0F, ent.shake - amount);
}

void UseEnt(Ent& ent, std::optional<VID> user_vid, AttachMode source) {
    const bool was_down = ent.use_state.down;
    ent.use_state.down = true;
    ent.use_state.pressed = !was_down;
    ent.use_state.released = false;
    ent.use_state.frames = was_down ? ent.use_state.frames + 1 : 1;
    ent.use_state.user_vid = user_vid;
    ent.use_state.source = source;
}

void PressUseEnt(Ent& ent, std::optional<VID> user_vid, AttachMode source) {
    UseEnt(ent, user_vid, source);
    ent.use_state.pressed = true;
}

void ReleaseUseEnt(Ent& ent, std::optional<VID> user_vid, AttachMode source) {
    ent.use_state.down = false;
    ent.use_state.pressed = false;
    ent.use_state.released = true;
    ent.use_state.frames = 0;
    ent.use_state.user_vid = user_vid;
    ent.use_state.source = source;
}

void StopUsingEnt(Ent& ent) {
    const bool was_down = ent.use_state.down;
    ent.use_state.down = false;
    ent.use_state.pressed = false;
    ent.use_state.released = was_down;
    ent.use_state.frames = 0;
    ent.use_state.user_vid.reset();
    ent.use_state.source = AttachMode::None;
}

bool HasMovementFlag(const Ent& ent, EntMovementFlag movement_flag) {
    return (ent.movement_flags & MovementFlagBit(movement_flag)) != 0;
}

void SetMovementFlag(Ent& ent, EntMovementFlag movement_flag, bool enabled) {
    if (enabled) {
        ent.movement_flags |= MovementFlagBit(movement_flag);
        return;
    }

    ent.movement_flags &= ~MovementFlagBit(movement_flag);
}

void ClearTransientMovementFlags(Ent& ent) {
    SetMovementFlag(ent, EntMovementFlag::Walking, false);
    SetMovementFlag(ent, EntMovementFlag::Running, false);
    SetMovementFlag(ent, EntMovementFlag::Pushing, false);
}

std::tuple<Vec2, Vec2> Ent::GetBounds() const {
    return {pos, pos + size - Vec2::New(1.0F, 1.0F)};
}

AABB Ent::GetAABB() const {
    return AABB::New(pos, pos + size - Vec2::New(1.0F, 1.0F));
}

Vec2 Ent::GetCenter() const {
    return pos + size / 2.0F;
}

void Ent::SetCenter(const Vec2& center) {
    pos = center - size / 2.0F;
}

void Ent::IncTravelSound() {
    switch (travel_sound) {
    case TravelSound::One:
        travel_sound = TravelSound::Two;
        return;
    case TravelSound::Two:
        travel_sound = TravelSound::One;
        return;
    }
}

bool Ent::IsHanging() const {
    return hang_side.has_value();
}

bool Ent::IsClimbing() const {
    return HasMovementFlag(*this, EntMovementFlag::Climbing);
}

AABB Ent::GetFeet() const {
    const auto [tl, br] = GetBounds();
    return AABB::New(Vec2::New(tl.x, br.y), br + Vec2::New(0.0F, 1.0F));
}

AABB Ent::GetGroundProbe() const {
    AABB feet = GetFeet();
    feet.br.y += kGroundProbeFractionalEpsilon;
    return feet;
}

bool Ent::TrySnapToBlockingStageBottom(const Stage& stage) {
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

void Ent::SetGrounded(const Stage& stage) {
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

std::tuple<Vec2, Vec2> Ent::GetTlAndTrCorners() const {
    return {Vec2::New(pos.x, pos.y), Vec2::New(pos.x + size.x, pos.y)};
}

HangHands Ent::GetHangHands() const {
    const auto [tl, tr] = GetTlAndTrCorners();
    HangHands hang_hands;
    hang_hands.left = tl;
    hang_hands.right = tr;
    return hang_hands;
}

HangHandBounds Ent::GetHangHandsBounds() const {
    const auto [tl, _br] = GetBounds();
    const Vec2 right_edge = tl + Vec2::New(size.x, 0.0F);
    HangHandBounds hang_hands;
    hang_hands.left_tl = tl - kHangHandSize;
    hang_hands.left_br = tl;
    hang_hands.right_tl = right_edge - Vec2::New(0.0F, kHangHandSize.y);
    hang_hands.right_br = right_edge + Vec2::New(kHangHandSize.x, 0.0F);
    return hang_hands;
}

bool TrySetAnim(Ent& ent, EntDisplayState display_state) {
    const auto selection = GetAFrameSelectionForDisplayState(EntDisplayInput{
        .type_ = ent.type_,
        .display_state = display_state,
    });
    if (!selection.has_value()) {
        return false;
    }

    SetAnim(ent, selection->anim_id);
    ent.aframe_animator.animate = selection->animate;
    if (selection->has_forced_frame) {
        ent.aframe_animator.SetForcedFrame(selection->forced_frame);
    }
    return true;
}

void SetAnim(Ent& ent, AFrameId anim_id) {
    ent.aframe_animator.SetAnim(anim_id);
}

bool TryCollectEffectPickup(Ent& ent, const Ent& pickup) {
    if (!pickup.pickup_effect.has_value()) {
        return false;
    }

    (void)AddEffect(ent, *pickup.pickup_effect, GetEffectSpec(*pickup.pickup_effect).default_count);
    return true;
}

bool TryCollectInventoryPickup(State& state, Ent& ent, const Ent& pickup) {
    bool collected = false;
    switch (pickup.type_) {
    case EntType::BombBox:
        collected |= state.ent_tools.AddToolCount(ent.vid, ToolKind::ThrowBomb, 12);
        break;
    case EntType::BombBag:
        collected |= state.ent_tools.AddToolCount(ent.vid, ToolKind::ThrowBomb, 3);
        break;
    case EntType::Paste:
        collected |= state.ent_tools.UpgradeBombsToSticky(ent.vid);
        break;
    case EntType::RopePile:
        collected |= state.ent_tools.AddToolCount(ent.vid, ToolKind::ThrowRope, 3);
        break;
    default:
        break;
    }
    if (TryCollectEffectPickup(ent, pickup)) {
        collected = true;
    }
    return collected;
}

bool CanRevealEmbeddedTreasure(const Ent& ent) {
    return GetModifiedEffectValue(ent, EffectModifierTarget::HiddenTreasureVisibility, 0.0F) > 0.0F;
}

void EnableStone(Ent& ent) {
    ent.stone = true;
    ent.crusher_pusher = true;
    ent.impassable = true;
    ent.damage_vuln = DamageVuln::ExplosionOnly;
}

void DisableStone(Ent& ent) {
    ent.stone = false;
    RestoreEntCrusherPusherFromSpec(ent);
    RestoreEntImpassableFromSpec(ent);
    RestoreEntDamageVulnFromSpec(ent);
}

} // namespace splonks

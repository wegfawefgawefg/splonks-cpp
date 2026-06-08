#pragma once

#include "ent/core_types.hpp"
#include "ent/callbacks.hpp"
#include "effects.hpp"
#include "aframe_animator.hpp"
#include "aframe_id.hpp"
#include "math_types.hpp"
#include "sim/fxp.hpp"
#include "stage_progression.hpp"
#include "stage.hpp"
#include "utils.hpp"

#include <array>
#include <optional>
#include <cstdint>
#include <tuple>
#include <vector>

namespace splonks {

constexpr std::uint32_t kJumpDelayFrames = 1;

struct UseState {
    bool down = false;
    bool pressed = false;
    bool released = false;
    std::uint32_t frames = 0;
    std::optional<VID> user_vid;
    AttachMode source = AttachMode::None;
};

struct Buyable {
    bool active = false;
    std::uint32_t display_quantity = 0;
    std::optional<AFrameId> display_icon_anim_id = std::nullopt;
    std::optional<VID> shop_owner_vid = std::nullopt;
    EntOnTryBuy on_try_buy = nullptr;
};

struct Ent {
    bool active = false;
    bool marked_for_destruction = false;
    EntType type_ = EntType::None;
    VID vid;
    bool has_physics = true;
    bool can_collide = true;
    bool can_be_hit = true;
    bool can_receive_proj_contact = true;
    bool stone = false;
    bool wanted = false;
    bool crusher_pusher = false;
    bool pushable = false;
    bool can_stomp = false;
    bool can_be_stomped = true;
    bool can_collect_pickups = false;
    bool can_go_on_back = false;
    bool grounded = false;
    sim::Scalar shake = sim::Scalar::zero();
    sim::Scalar rotation = sim::Scalar::zero();
    sim::Scalar alpha = sim::Scalar::from_int(1);
    std::uint32_t coyote_time = 0;
    std::uint32_t stun_timer = 0;
    bool stun_recovers_on_ground = true;
    bool stun_recovers_while_held = true;
    bool can_be_picked_up = true;
    bool affected_by_cobweb = true;
    bool can_only_be_picked_up_if_dead_or_stunned = false;
    bool impassable = false;
    bool can_be_hung_on = true;
    std::uint32_t fall_timer = 0;
    sim::Vec2 pos;
    sim::Vec2 vel;
    sim::Vec2 acc;
    sim::Scalar max_speed = sim::ToSimScalar(7.0F);
    std::uint32_t jump_hold_gravity_frames_remaining = 0;
    sim::Scalar throw_velocity_scale = sim::Scalar::from_int(1);
    sim::Scalar buoyancy = sim::Scalar::zero();
    sim::Vec2 size;
    sim::Scalar self_light = sim::Scalar::zero();
    sim::Scalar light_strength = sim::Scalar::zero();
    sim::Color3 light_color = sim::ToSimColor3(Color3::White());
    int light_radius = 0;
    sim::Scalar dist_traveled_this_frame = sim::Scalar::zero();
    Side facing = Side::Left;
    bool vertical_flip = false;
    DrawLayer draw_layer = DrawLayer::Middle;
    bool render_enabled = true;
    AFrameAnimator aframe_animator;
    std::uint32_t jump_delay_frame_count = kJumpDelayFrames;
    bool jumped_this_frame = false;
    std::uint32_t climb_detach_cooldown = 0;
    std::optional<Side> hang_side;
    bool can_hang_ledge = false;
    bool can_hang_wall = false;
    std::uint32_t hang_count = 0;
    bool holding = false;
    BoxedEntEffects effects;
    std::optional<EffectId> pickup_effect = std::nullopt;
    std::uint32_t money = 0;
    Buyable buyable;
    std::optional<std::uint32_t> stage_spawn_index;
    std::optional<VID> back_vid;
    AttachMode attach_mode = AttachMode::None;
    UseState use_state;
    sim::Scalar travel_sound_countdown =
        sim::Scalar::from_int(static_cast<std::int32_t>(kTravelSoundDistInterval));
    TravelSound travel_sound = TravelSound::One;
    EntCondition condition = EntCondition::Normal;
    EntCondition last_condition = EntCondition::Normal;
    EntAiState ai_state = EntAiState::Idle;
    EntAiState last_ai_state = EntAiState::Idle;
    std::uint32_t movement_flags = 0;
    std::uint32_t health = 0;
    bool hurt_on_contact = false;
    bool vanish_on_death = false;
    bool affected_by_ground_friction = true;
    sim::Scalar support_ground_friction = sim::ToSimScalar(0.85F);
    sim::Scalar push_acc = sim::Scalar::zero();
    std::optional<AFrameId> damage_anim = std::nullopt;
    std::optional<AudioAssetId> damage_sound = std::nullopt;
    std::optional<AudioAssetId> collide_sound = std::nullopt;
    std::optional<AudioAssetId> death_sound = std::nullopt;
    EntOnDeath on_death = nullptr;
    EntOnDamage on_damage = nullptr;
    EntOnUse on_use = nullptr;
    EntOnAreaEnter on_area_enter = nullptr;
    EntOnAreaExit on_area_exit = nullptr;
    EntOnAreaTileChanged on_area_tile_changed = nullptr;
    EntControlLogic control_logic = nullptr;
    EntStepLogic step_logic = nullptr;
    EntStepPhysics step_physics = nullptr;
    std::optional<StageTransitionTarget> transition_target;
    StageExitId stage_exit_id = kInvalidStageExitId;
    std::uint32_t bomb_throw_delay_countdown = 0;
    std::uint32_t rope_throw_delay_countdown = 0;
    std::uint32_t attack_delay_countdown = 0;
    std::uint32_t equip_delay_countdown = 0;
    std::optional<VID> thrown_by;
    std::uint32_t thrown_immunity_timer = 0;
    DamageType proj_contact_damage_type = DamageType::Attack;
    std::uint32_t proj_contact_damage_amount = 1;
    bool can_apply_proj_contact = true;
    std::uint32_t proj_contact_timer = 0;
    bool collided = false;
    bool collided_last_frame = false;
    std::uint32_t contact_sound_cooldown = 0;
    DamageVuln damage_vuln = DamageVuln::Vulnerable;
    bool can_be_stunned = false;
    IVec2 point_a;
    IVec2 point_b;
    IVec2 point_c;
    IVec2 point_d;
    PointLabel point_label_a = PointLabel::None;
    PointLabel point_label_b = PointLabel::None;
    PointLabel point_label_c = PointLabel::None;
    PointLabel point_label_d = PointLabel::None;
    std::optional<VID> holding_vid;
    std::optional<VID> held_by_vid;
    std::uint32_t holding_timer = kDefaultHoldingTimer;
    std::optional<VID> ent_a;
    std::optional<VID> ent_b;
    std::optional<VID> ent_c;
    std::optional<VID> ent_d;
    std::optional<std::vector<VID>> child_vids;
    std::optional<std::vector<VID>> inside_vids;
    EntLabel ent_label_a = EntLabel::None;
    Alignment alignment = Alignment::Neutral;
    sim::Scalar counter_a = sim::Scalar::zero();
    sim::Scalar counter_b = sim::Scalar::zero();
    sim::Scalar counter_c = sim::Scalar::zero();
    sim::Scalar counter_d = sim::Scalar::zero();
    sim::Scalar threshold_a = sim::Scalar::zero();
    sim::Scalar threshold_b = sim::Scalar::zero();

    static constexpr Vec2 kHangHandSize = {1.0F, 4.0F};

    static Ent New();
    void Reset();
    sim::Vec2 GetSimPos() const;
    sim::Vec2 GetSimVel() const;
    sim::Vec2 GetSimAcc() const;
    void SetSimPos(sim::Vec2 value);
    void SetSimVel(sim::Vec2 value);
    void SetSimAcc(sim::Vec2 value);
    Vec2 GetRenderPos() const;
    Vec2 GetRenderVel() const;
    Vec2 GetRenderAcc() const;
    void SetRenderPos(const Vec2& value);
    void SetRenderVel(const Vec2& value);
    void SetRenderAcc(const Vec2& value);
    Vec2 GetSize() const;
    sim::AABB GetSimAABB() const;
    sim::Vec2 GetSimCenter() const;
    void SetSimCenter(sim::Vec2 center);
    sim::AABB GetSimFeet() const;
    sim::AABB GetSimGroundProbe() const;
    std::tuple<Vec2, Vec2> GetRenderBounds() const;
    RenderAABB GetRenderAABB() const;
    Vec2 GetRenderCenter() const;
    void SetRenderCenter(const Vec2& center);
    void IncTravelSound();
    bool IsHanging() const;
    bool IsClimbing() const;
    bool TrySnapToBlockingStageBottom(const Stage& stage);
    void SetGrounded(const Stage& stage);
    std::tuple<Vec2, Vec2> GetTlAndTrCorners() const;
    HangHands GetHangHands() const;
    HangHandBounds GetHangHandsBounds() const;
};

// Raw anim path.
// Use this when ent-owned logic knows the exact authored anim id it wants.
// This does not change semantic display state.
void SetAnim(Ent& ent, AFrameId anim_id);
// Semantic anim path.
// Use this from shared/external gameplay code that only knows a generic display state
// like Neutral, Walk, Hanging, or Stunned rather than an exact authored anim id.
bool TrySetAnim(Ent& ent, EntDisplayState display_state);
void UseEnt(Ent& ent, std::optional<VID> user_vid, AttachMode source);
void PressUseEnt(Ent& ent, std::optional<VID> user_vid, AttachMode source);
void ReleaseUseEnt(Ent& ent, std::optional<VID> user_vid, AttachMode source);
void StopUsingEnt(Ent& ent);
bool HasMovementFlag(const Ent& ent, EntMovementFlag movement_flag);
void SetMovementFlag(Ent& ent, EntMovementFlag movement_flag, bool enabled);
void ClearTransientMovementFlags(Ent& ent);
bool TryCollectEffectPickup(Ent& ent, const Ent& pickup);
bool TryCollectInventoryPickup(State& state, Ent& ent, const Ent& pickup);
bool CanRevealEmbeddedTreasure(const Ent& ent);
void EnableStone(Ent& ent);
void DisableStone(Ent& ent);
void AddEntShake(Ent& ent, float amount);
void AttenuateEntShake(Ent& ent, float amount);

} // namespace splonks

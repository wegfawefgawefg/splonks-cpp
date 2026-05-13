#pragma once

#include "ent/core_types.hpp"
#include "ent/callbacks.hpp"
#include "effects.hpp"
#include "aframe_animator.hpp"
#include "aframe_id.hpp"
#include "math_types.hpp"
#include "stage_progression.hpp"
#include "stage.hpp"
#include "utils.hpp"

#include <array>
#include <optional>
#include <cstdint>
#include <tuple>
#include <vector>

namespace splonks {

constexpr unsigned int kJumpDelayFrames = 1;

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
    float shake = 0.0F;
    float rotation = 0.0F;
    float alpha = 1.0F;
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
    Vec2 pos;
    Vec2 vel;
    Vec2 acc;
    float max_speed = 7.0F;
    std::uint32_t jump_hold_gravity_frames_remaining = 0;
    float throw_velocity_scale = 1.0F;
    float buoyancy = 0.0F;
    Vec2 size;
    float self_light = 0.0F;
    float light_strength = 0.0F;
    Color3 light_color = Color3::White();
    int light_radius = 0;
    float dist_traveled_this_frame = 0.0F;
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
    std::optional<std::size_t> stage_spawn_index;
    std::optional<VID> back_vid;
    AttachMode attach_mode = AttachMode::None;
    UseState use_state;
    float travel_sound_countdown = kTravelSoundDistInterval;
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
    float support_ground_friction = 0.85F;
    float push_acc = 0.0F;
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
    float attack_weight = 0.0F;
    float weight = 0.0F;
    std::uint32_t bomb_throw_delay_countdown = 0;
    std::uint32_t rope_throw_delay_countdown = 0;
    std::uint32_t attack_delay_countdown = 0;
    std::uint32_t equip_delay_countdown = 0;
    std::optional<VID> thrown_by;
    std::uint32_t thrown_immunity_timer = 0;
    DamageType proj_contact_damage_type = DamageType::Attack;
    unsigned int proj_contact_damage_amount = 1;
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
    float counter_a = 0.0F;
    float counter_b = 0.0F;
    float counter_c = 0.0F;
    float counter_d = 0.0F;
    float threshold_a = 0.0F;
    float threshold_b = 0.0F;

    static constexpr Vec2 kHangHandSize = {1.0F, 4.0F};

    static Ent New();
    void Reset();
    std::tuple<Vec2, Vec2> GetBounds() const;
    AABB GetAABB() const;
    Vec2 GetCenter() const;
    void SetCenter(const Vec2& center);
    void IncTravelSound();
    bool IsHanging() const;
    bool IsClimbing() const;
    AABB GetFeet() const;
    AABB GetGroundProbe() const;
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

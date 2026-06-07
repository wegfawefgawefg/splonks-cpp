#include "ents/player.hpp"
#include "audio.hpp"
#include "ents/baseball_bat.hpp"
#include "ents/block.hpp"
#include "ents/common/common.hpp"
#include "ents/gear_items.hpp"
#include "ents/meathead.hpp"
#include "aframe_id.hpp"
#include "particles/sprite_particle.hpp"
#include "state.hpp"
#include "controls.hpp"
#include "world_ops.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace splonks::ents::player {

namespace {

constexpr float kPunishBallHeldMaxWalkSpeed = 1.75F;
constexpr float kPunishBallDraggedMaxWalkSpeed = 2.0F;
constexpr float kPunishBallHeldMaxRunSpeed = 2.5F;
constexpr float kPunishBallDraggedMaxRunSpeed = 3.0F;
constexpr float kPunishBallDraggedExtraGravity = 0.14F;
constexpr float kPunishBallDraggedJumpImpulse = 3.0F;
constexpr float kClimbAnimVelocityEpsilon = 0.01F;
constexpr std::uint32_t kClassicFallDamageMinFrames = 32;
constexpr std::uint32_t kClassicFallDamageMediumFrames = 64;
constexpr std::uint32_t kClassicFallDamageHeavyFrames = 96;
constexpr std::uint32_t kFallDamageLightAmount = 1;
constexpr std::uint32_t kFallDamageMediumAmount = 2;
constexpr std::uint32_t kFallDamageHeavyAmount = 10;
constexpr float kFallDamageBounceVelocityY = -3.0F;
constexpr float kPlayerWalkAnimSpeed = 1.0F;
constexpr float kPlayerRunAnimSpeed = 2.0F;
constexpr float kPlayerBlockedPushAnimSpeed = 0.35F;
constexpr float kPlayerWalkAnimVelocityEpsilon = 0.05F;
constexpr float kPlayerWalkAnimDistanceEpsilon = 0.0F;

struct PlayerControlTuning {
    float move_acc = kMoveAcc;
    float run_acc = kRunAcc;
};

struct PlayerPhysicsTuning {
    common::JumpAndClimbTuning jump_and_climb{};
    float max_walk_speed = kMaxWalkSpeed;
    float max_run_speed = kMaxRunSpeed;
    float max_speed = kMaxSpeed;
    float air_friction = 0.85F;
    float ground_friction_scale = 1.0F;
    std::uint32_t fall_damage_min_frames = kClassicFallDamageMinFrames;
    std::uint32_t fall_damage_medium_frames = kClassicFallDamageMediumFrames;
    std::uint32_t fall_damage_heavy_frames = kClassicFallDamageHeavyFrames;
};

std::uint32_t ClampTuningFrames(int value) {
    return static_cast<std::uint32_t>(std::max(0, value));
}

PlayerControlTuning MakePlayerControlTuning(const PlayerTuningState& tuning) {
    return PlayerControlTuning{
        .move_acc = tuning.move_acc,
        .run_acc = tuning.run_acc,
    };
}

PlayerPhysicsTuning MakePlayerPhysicsTuning(const PlayerTuningState& tuning) {
    return PlayerPhysicsTuning{
        .jump_and_climb = common::JumpAndClimbTuning{
            .gravity_scale = tuning.gravity_scale,
            .jump_impulse = tuning.jump_impulse,
            .spring_shoes_jump_impulse_bonus = tuning.spring_shoes_jump_impulse_bonus,
            .climb_speed = tuning.climb_speed,
            .climb_depart_horizontal_speed = tuning.climb_depart_horizontal_speed,
            .climb_probe_bias_pixels = tuning.climb_probe_bias_pixels,
            .climb_probe_x_scale = tuning.climb_probe_x_scale,
            .climb_required_probe_hits = ClampTuningFrames(tuning.climb_required_probe_hits),
            .coyote_time_frames = ClampTuningFrames(tuning.coyote_frames),
            .jump_delay_frames = ClampTuningFrames(tuning.jump_delay_frames),
            .jump_hold_gravity_frames = ClampTuningFrames(tuning.jump_hold_frames),
            .climb_detach_cooldown_frames = ClampTuningFrames(tuning.climb_detach_cooldown),
            .hang_drop_cooldown_frames = ClampTuningFrames(tuning.hang_drop_cooldown),
            .glove_hang_drop_cooldown_frames = ClampTuningFrames(tuning.glove_hang_drop_cooldown),
            .hang_wall_release_cooldown_frames = ClampTuningFrames(tuning.hang_wall_release_cooldown),
            .auto_ledge_grab = tuning.auto_ledge_grab,
        },
        .max_walk_speed = tuning.walk_speed,
        .max_run_speed = tuning.run_speed,
        .max_speed = tuning.max_fall_speed,
        .air_friction = tuning.air_friction,
        .ground_friction_scale = tuning.ground_friction_scale,
        .fall_damage_min_frames = ClampTuningFrames(tuning.fall_damage_light_frames),
        .fall_damage_medium_frames = ClampTuningFrames(tuning.fall_damage_medium_frames),
        .fall_damage_heavy_frames = ClampTuningFrames(tuning.fall_damage_heavy_frames),
    };
}

bool PlayerHasPunishBall(const Ent& player, const State& state) {
    if (!player.ent_d.has_value()) {
        return false;
    }

    const Ent* const ball = state.ents.GetEnt(*player.ent_d);
    return ball != nullptr && ball->active && ball->type_ == EntType::BallAndChainBall;
}

bool PlayerIsHoldingPunishBall(const Ent& player, const State& state) {
    return player.holding_vid.has_value() && player.ent_d.has_value() &&
           *player.holding_vid == *player.ent_d && PlayerHasPunishBall(player, state);
}

bool IsExternallyLaunchedPlayer(const Ent& player) {
    return player.proj_contact_timer > 0 &&
           !player.held_by_vid.has_value() &&
           player.attach_mode == AttachMode::None;
}

bool CanPlayerEmote(const Ent& player, const controls::ControlIntent& control) {
    return player.condition == EntCondition::Normal &&
           player.grounded &&
           !player.IsClimbing() &&
           !player.IsHanging() &&
           !control.left &&
           !control.right &&
           !control.jump;
}

bool TryStepPlayerEmote(Ent& player, const controls::ControlIntent& control) {
    if (!CanPlayerEmote(player, control)) {
        return false;
    }

    if (control.emote_up) {
        TrySetAnim(player, EntDisplayState::EmoteBald);
        return true;
    }
    if (control.emote_down) {
        TrySetAnim(player, EntDisplayState::EmoteDab);
        return true;
    }

    return false;
}

void UpdateClimbAnimPlayback(Ent& player, const Graphics& graphics) {
    if (player.aframe_animator.anim_id != aframe_ids::PlayerClimbing) {
        return;
    }

    AFrameAnimator& animator = player.aframe_animator;
    animator.loop = true;
    animator.ResetSpeed();
    animator.finished = false;

    if (std::abs(player.vel.y) <= kClimbAnimVelocityEpsilon) {
        animator.animate = false;
        return;
    }

    animator.animate = true;
    const AnimPlaybackMode desired_mode =
        player.vel.y < 0.0F ? AnimPlaybackMode::Forward : AnimPlaybackMode::Reverse;
    if (animator.playback_mode == desired_mode) {
        return;
    }

    const AFrameAnim* const anim = graphics.aframe_db.FindAnim(animator.anim_id);
    if (anim != nullptr && animator.current_frame < anim->frame_indices.size()) {
        const AFrame& aframe =
            graphics.aframe_db.frames[anim->frame_indices[animator.current_frame]];
        const sim::Scalar frame_duration =
            sim::Scalar::from_int(static_cast<std::int32_t>(aframe.duration));
        animator.current_time =
            std::clamp(frame_duration - animator.current_time, sim::Scalar::zero(), frame_duration);
    }

    animator.playback_mode = desired_mode;
    animator.playback_dirty = false;
}

void StepPlayerFallTimer(Ent& player, const State& state) {
    const float fall_timer_rate =
        GetModifiedEffectValue(player, EffectModifierTarget::FallTimerRate, 1.0F, &state);
    if (fall_timer_rate <= 0.0F) {
        player.fall_timer = 0;
        return;
    }
    if (player.vel.y > 0.0F && !player.IsClimbing() && !player.IsHanging()) {
        if (fall_timer_rate >= 1.0F) {
            player.fall_timer += static_cast<std::uint32_t>(RoundToInt(fall_timer_rate));
            return;
        }
        const std::uint32_t interval =
            static_cast<std::uint32_t>(std::max(1, RoundToInt(1.0F / fall_timer_rate)));
        if ((state.stage_frame % interval) == 0) {
            player.fall_timer += 1;
        }
        return;
    }

    player.fall_timer = 0;
}

float GetFallDamageTimer(const Ent& player, const State& state) {
    (void)state;
    return static_cast<float>(player.fall_timer);
}

std::uint32_t GetFallDamageAmount(
    const PlayerPhysicsTuning& tuning,
    float fall_damage_timer
) {
    if (fall_damage_timer > static_cast<float>(tuning.fall_damage_heavy_frames)) {
        return kFallDamageHeavyAmount;
    }
    if (fall_damage_timer > static_cast<float>(tuning.fall_damage_medium_frames)) {
        return kFallDamageMediumAmount;
    }
    return kFallDamageLightAmount;
}

void SpawnFallDamagePoofs(const Ent& player, State& state) {
    const Vec2 base_pos = player.GetCenter() + Vec2::New(0.0F, player.size.y * 0.5F);
    for (float direction : {-1.0F, 1.0F}) {
        SpriteParticle smoke{};
        smoke.aframe_animator = AFrameAnimator::New(aframe_ids::LittleSmoke);
        smoke.draw_layer = DrawLayer::Foreground;
        smoke.counter = 16;
        smoke.pos = base_pos + Vec2::New(direction * 4.0F, -2.0F);
        smoke.size = Vec2::New(5.0F, 5.0F);
        smoke.alpha = 0.85F;
        smoke.vel = Vec2::New(direction * 0.12F, -0.08F);
        smoke.svel = Vec2::New(0.08F, 0.08F);
        smoke.alpha_vel = -0.05F;
        state.particles.Add(std::move(smoke));
    }
}

void ApplyClassicFallDamageOnLanding(
    std::size_t ent_idx,
    State& state,
    Audio& audio,
    const PlayerPhysicsTuning& tuning
) {
    Ent& player = state.ents.ents[ent_idx];
    if (!player.grounded) {
        return;
    }

    const float fall_damage_timer = GetFallDamageTimer(player, state);
    const bool was_holding_player =
        player.holding_vid.has_value() &&
        state.players.FindByEntVid(*player.holding_vid) != nullptr;
    player.fall_timer = 0;
    if (fall_damage_timer <= static_cast<float>(tuning.fall_damage_min_frames) ||
        player.condition == EntCondition::Dead) {
        return;
    }

    const std::uint32_t damage_amount = GetFallDamageAmount(tuning, fall_damage_timer);
    const common::DamageResult damage_result =
        common::TryDamageEnt(ent_idx, state, audio, DamageType::Fall, damage_amount);
    if (damage_result == common::DamageResult::None) {
        return;
    }

    Ent& mutable_player = state.ents.ents[ent_idx];
    if (was_holding_player) {
        mutable_player.vel.y = 0.0F;
        mutable_player.grounded = true;
    } else {
        mutable_player.vel.y = kFallDamageBounceVelocityY;
        mutable_player.grounded = false;
    }
    SpawnFallDamagePoofs(mutable_player, state);
    (void)PlayEntCenterSoundEmitter(state, mutable_player, audio_asset_ids::Thud);
}

void ControlEntAsPlayerWithTuning(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt,
    const PlayerControlTuning& tuning
) {
    (void)graphics;
    (void)audio;
    (void)dt;
    if (ent_idx >= state.ents.ents.size()) {
        return;
    }

    Ent& player = state.ents.ents[ent_idx];
    const controls::ControlIntent intent = controls::GetControlIntentForEnt(player, state);
    if (player.condition != EntCondition::Normal) {
        return;
    }

    const bool climbing = player.IsClimbing();
    if (climbing) {
        player.acc.x = 0.0F;
        player.vel.x = 0.0F;
    } else if (!(intent.left && intent.right)) {
        if (intent.run) {
            if (intent.left) {
                player.acc.x = -tuning.run_acc;
            }
            if (intent.right) {
                player.acc.x = tuning.run_acc;
            }
        } else {
            if (intent.left) {
                player.acc.x = -tuning.move_acc;
            }
            if (intent.right) {
                player.acc.x = tuning.move_acc;
            }
        }
    }
    if (intent.stop) {
        player.acc = Vec2::New(0.0F, 0.0F);
        player.vel = Vec2::New(0.0F, 0.0F);
    }
}

} // namespace

void ControlEntAsPlayer(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    ControlEntAsPlayerWithTuning(
        ent_idx,
        state,
        graphics,
        audio,
        dt,
        MakePlayerControlTuning(state.player_tuning)
    );
}

extern const EntSpec kPlayerSpec{
    .type_ = EntType::Player,
    .size = Vec2::New(10.0F, 10.0F),
    .health = 400,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .can_collect_pickups = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_stomp = true,
    .can_hang_ledge = true,
    .can_be_stunned = true,
    .stun_recovers_on_ground = true,
    .stun_recovers_while_held = false,
    .draw_layer = DrawLayer::Middle,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::Vulnerable,
    .damage_anim = aframe_ids::BloodBall,
    .damage_sound = audio_asset_ids::PlayerOuch,
    .control_logic = ControlEntAsPlayer,
    .step_logic = StepEntLogicAsPlayer,
    .step_physics = StepEntPhysicsAsPlayer,
    .alignment = Alignment::Ally,
    .aframe_animator = AFrameAnimator::New(aframe_ids::PlayerStanding),
};

void StepEntLogicAsPlayer(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)dt;
    {
        // SKIP CONDITIONS
        Ent& player = state.ents.ents[ent_idx];
        const EntCondition player_condition = player.condition;
        if (player_condition == EntCondition::Dead) {
            (void)common::SeverEntOutboundCarryLinksForReset(player, state);
            gear_items::ClearEquippedPassiveItemVisuals(player, state, graphics);

            return;
        }
    }

    //  REQUIRED HACK FOR JETPACK TO CATCH GROUND TOUCH ON FRAME JUMP
    {
        Ent& player = state.ents.ents[ent_idx];
        player.jumped_this_frame = false;
    }

    // TODO: probably put a check for dead or stunned up here lol
    common::StepTravelSoundWalkerClimber(ent_idx, state, audio);
    common::CleanupInactiveCarryReferences(ent_idx, state);

    {
        const Ent& player = state.ents.ents[ent_idx];
        meathead::MaybePreviewMeatheadPassive(player, state);
    }

    const bool loss_of_control =
        state.ents.ents[ent_idx].condition == EntCondition::Stunned;
    const controls::ControlIntent control =
        controls::GetControlIntentForEnt(
            state.ents.ents[ent_idx],
            state
        );

    {
        Ent& player = state.ents.ents[ent_idx];
        const bool hanging = player.IsHanging();
        const bool climbing = player.IsClimbing();
        const bool walking =
            !loss_of_control &&
            (control.left != control.right) &&
            player.grounded &&
            !climbing &&
            !hanging;
        SetMovementFlag(player, EntMovementFlag::Walking, walking);
        SetMovementFlag(player, EntMovementFlag::Running, walking && control.run);
        SetMovementFlag(player, EntMovementFlag::Climbing, climbing);
        SetMovementFlag(player, EntMovementFlag::Hanging, hanging);
    }

    // SET ANIMATIONS AND DISPLAY STATES
    {
        Ent& player = state.ents.ents[ent_idx];
        if (player.vel.x < 0.0F) {
            player.facing = Side::Left;
        }
        if (player.vel.x > 0.0F) {
            player.facing = Side::Right;
        }

        // skip all actions
        if (!loss_of_control) {
            // Hanging and climbing must win before locomotion. Otherwise walk/neutral
            // gets assigned first and the climb anim restarts every tick.
            if (player.hang_side == Side::Left) {
                TrySetAnim(player, EntDisplayState::Hanging);
                player.facing = Side::Left;
            } else if (player.hang_side == Side::Right) {
                TrySetAnim(player, EntDisplayState::Hanging);
                player.facing = Side::Right;
            } else if (player.IsClimbing()) {
                if (player.aframe_animator.anim_id != aframe_ids::PlayerClimbing) {
                    player.aframe_animator.PlayLoop(aframe_ids::PlayerClimbing);
                }
                player.aframe_animator.loop = true;
                player.aframe_animator.finished = false;
            } else if (TryStepPlayerEmote(player, control)) {
                player.aframe_animator.ResetSpeed();
            } else {
                const bool holding_or_pushing =
                    player.holding ||
                    player.holding_vid.has_value() ||
                    HasMovementFlag(player, EntMovementFlag::Pushing);
                const bool has_horizontal_input = control.left != control.right;
                if (has_horizontal_input) {
                    player.facing = control.left ? Side::Left : Side::Right;
                }
                const bool moving_with_input =
                    (control.left && player.vel.x < -kPlayerWalkAnimVelocityEpsilon) ||
                    (control.right && player.vel.x > kPlayerWalkAnimVelocityEpsilon);
                const bool walking_horizontally =
                    player.grounded &&
                    moving_with_input &&
                    player.dist_traveled_this_frame > sim::Scalar::zero();
                const bool running_horizontally = walking_horizontally && control.run;
                const bool pushing_into_blocker =
                    player.grounded &&
                    has_horizontal_input &&
                    !walking_horizontally;

                if (!player.grounded && player.vel.y > 0.0F) {
                    TrySetAnim(player, EntDisplayState::Falling);
                    player.aframe_animator.ResetSpeed();
                } else if (walking_horizontally) {
                    TrySetAnim(
                        player,
                        holding_or_pushing ? EntDisplayState::WalkHolding : EntDisplayState::Walk
                    );
                    player.aframe_animator.SetSpeed(
                        running_horizontally ? kPlayerRunAnimSpeed : kPlayerWalkAnimSpeed
                    );
                } else if (pushing_into_blocker) {
                    TrySetAnim(player, EntDisplayState::WalkHolding);
                    player.aframe_animator.SetSpeed(kPlayerBlockedPushAnimSpeed);
                } else {
                    TrySetAnim(
                        player,
                        holding_or_pushing ? EntDisplayState::NeutralHolding : EntDisplayState::Neutral
                    );
                    player.aframe_animator.ResetSpeed();
                }
            }
        }
    }

    common::UpdateCarryAndBackItems(ent_idx, state, graphics, audio);

    // PLAYER TOOL SLOT 1
    if (!loss_of_control) {
        common::TryUseToolSlot(ent_idx, state, graphics, audio, 0, control.bomb_pressed);
    }

    // PLAYER TOOL SLOT 2
    if (!loss_of_control) {
        common::TryUseToolSlot(ent_idx, state, graphics, audio, 1, control.rope_pressed);
    }

    // PLAYER SWING BAT SECTION
    {
        Ent& player = state.ents.ents[ent_idx];
        if (player.attack_delay_countdown > 0) {
            player.attack_delay_countdown -= 1;
        }
    }
    if (!loss_of_control) {
        const Ent& player = state.ents.ents[ent_idx];
        const Vec2 player_pos = player.pos;
        const bool trying_to_attack = control.attack_pressed;
        const VID player_vid = player.vid;
        const unsigned int attack_delay_countdown = player.attack_delay_countdown;
        const bool has_held_item = player.holding_vid.has_value();

        bool attacked = false;
        if (trying_to_attack && attack_delay_countdown == 0 && !has_held_item) {
            if (world_ops::SpawnEnt(
                    state,
                    EntType::BaseballBat,
                    [&](Ent& ent) {
                        ent.pos = player_pos;
                        ent.held_by_vid = player_vid;
                        ent.attach_mode = AttachMode::Held;
                        state.UpdateSidForEnt(ent.vid.id, graphics);
                    },
                    player_vid
                ) != nullptr) {
                attacked = true;
                if (const Ent* const sound_player = state.ents.GetEnt(player_vid)) {
                    (void)PlayEntSoundEmitter(state, *sound_player, audio_asset_ids::BaseballBatSwing);
                }
            }
        }
        if (attacked) {
            Ent& mutable_player = state.ents.ents[ent_idx];
            mutable_player.attack_delay_countdown = kAttackDelay;
        }
    }

    // PUSH BLOCKS
    if (!loss_of_control) {
        common::TryPushBlocks(ent_idx, state, graphics);
    }

}

namespace {

void StepEntPhysicsAsPlayerWithTuning(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt,
    const PlayerPhysicsTuning& tuning
) {
    common::HangHandsStep(ent_idx, state, tuning.jump_and_climb);
    common::JumpingAndClimbingStep(ent_idx, state, audio, tuning.jump_and_climb);

    // custom pre partial euler step for player to apply special velocity clamping.
    Ent& ent = state.ents.ents[ent_idx];
    const bool externally_launched_player = IsExternallyLaunchedPlayer(ent);
    const bool has_punish_ball = PlayerHasPunishBall(ent, state);
    const bool holding_punish_ball = PlayerIsHoldingPunishBall(ent, state);
    if (has_punish_ball && !holding_punish_ball && !ent.IsClimbing()) {
        ent.acc.y += kPunishBallDraggedExtraGravity;
    }

    if (ent.IsClimbing()) {
        UpdateClimbAnimPlayback(ent, graphics);
    }

    ent.vel += ent.acc;
    if (has_punish_ball && !holding_punish_ball && ent.jumped_this_frame &&
        ent.vel.y < -kPunishBallDraggedJumpImpulse) {
        ent.vel.y = -kPunishBallDraggedJumpImpulse;
    }
    const controls::ControlIntent control =
        controls::GetControlIntentForEnt(ent, state);
    const float max_walk_speed =
        holding_punish_ball ? kPunishBallHeldMaxWalkSpeed
                            : (has_punish_ball ? kPunishBallDraggedMaxWalkSpeed : tuning.max_walk_speed);
    const float max_run_speed =
        holding_punish_ball ? kPunishBallHeldMaxRunSpeed
                            : (has_punish_ball ? kPunishBallDraggedMaxRunSpeed : tuning.max_run_speed);
    const float move_speed_scale = std::max(
        0.0F,
        GetModifiedEffectValue(ent, EffectModifierTarget::MoveSpeedScale, 1.0F, &state)
    );
    if (!externally_launched_player) {
        if (control.run) {
            ent.vel.x = std::clamp(ent.vel.x, -max_run_speed * move_speed_scale, max_run_speed * move_speed_scale);
        } else {
            ent.vel.x =
                std::clamp(ent.vel.x, -max_walk_speed * move_speed_scale, max_walk_speed * move_speed_scale);
        }
    }
    const float max_fall_speed =
        GetModifiedEffectValue(ent, EffectModifierTarget::MaxFallSpeed, tuning.max_speed, &state);
    ent.vel.y = std::min(ent.vel.y, max_fall_speed);
    StepPlayerFallTimer(ent, state);
    gear_items::StepEquippedPassiveItems(ent_idx, state, graphics);

    common::DoTileAndEntCollisions(ent_idx, state, graphics, audio);
    common::ApplySpecGroundFriction(ent_idx, state, tuning.ground_friction_scale);
    if (!ent.grounded && !externally_launched_player) {
        ent.vel.x *= tuning.air_friction;
    }
    ApplyClassicFallDamageOnLanding(ent_idx, state, audio, tuning);
    common::PostPartialEulerStep(ent_idx, state, dt);
}

} // namespace

/** generalize this to all square or rectangular ents somehow */
void StepEntPhysicsAsPlayer(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    StepEntPhysicsAsPlayerWithTuning(
        ent_idx,
        state,
        graphics,
        audio,
        dt,
        MakePlayerPhysicsTuning(state.player_tuning)
    );
}

} // namespace splonks::ents::player

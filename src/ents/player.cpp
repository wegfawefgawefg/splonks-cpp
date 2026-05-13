#include "entities/player.hpp"
#include "audio.hpp"
#include "entities/baseball_bat.hpp"
#include "entities/block.hpp"
#include "entities/common/common.hpp"
#include "entities/gear_items.hpp"
#include "entities/meathead.hpp"
#include "frame_data_id.hpp"
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

namespace splonks::entities::player {

namespace {

constexpr float kPunishBallHeldMaxWalkSpeed = 1.75F;
constexpr float kPunishBallDraggedMaxWalkSpeed = 2.0F;
constexpr float kPunishBallHeldMaxRunSpeed = 2.5F;
constexpr float kPunishBallDraggedMaxRunSpeed = 3.0F;
constexpr float kPunishBallDraggedExtraGravity = 0.14F;
constexpr float kPunishBallDraggedJumpImpulse = 3.0F;
constexpr float kClimbAnimationVelocityEpsilon = 0.01F;
constexpr std::uint32_t kClassicFallDamageMinFrames = 32;
constexpr std::uint32_t kClassicFallDamageMediumFrames = 64;
constexpr std::uint32_t kClassicFallDamageHeavyFrames = 96;
constexpr unsigned int kFallDamageLightAmount = 1;
constexpr unsigned int kFallDamageMediumAmount = 2;
constexpr unsigned int kFallDamageHeavyAmount = 10;
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

bool PlayerHasPunishBall(const Entity& player, const State& state) {
    if (!player.entity_d.has_value()) {
        return false;
    }

    const Entity* const ball = state.entity_manager.GetEntity(*player.entity_d);
    return ball != nullptr && ball->active && ball->type_ == EntityType::BallAndChainBall;
}

bool PlayerIsHoldingPunishBall(const Entity& player, const State& state) {
    return player.holding_vid.has_value() && player.entity_d.has_value() &&
           *player.holding_vid == *player.entity_d && PlayerHasPunishBall(player, state);
}

bool IsExternallyLaunchedPlayer(const Entity& player) {
    return player.projectile_contact_timer > 0 &&
           !player.held_by_vid.has_value() &&
           player.attachment_mode == AttachmentMode::None;
}

bool CanPlayerEmote(const Entity& player, const controls::ControlIntent& control) {
    return player.condition == EntityCondition::Normal &&
           player.grounded &&
           !player.IsClimbing() &&
           !player.IsHanging() &&
           !control.left &&
           !control.right &&
           !control.jump;
}

bool TryStepPlayerEmote(Entity& player, const controls::ControlIntent& control) {
    if (!CanPlayerEmote(player, control)) {
        return false;
    }

    if (control.emote_up) {
        TrySetAnimation(player, EntityDisplayState::EmoteBald);
        return true;
    }
    if (control.emote_down) {
        TrySetAnimation(player, EntityDisplayState::EmoteDab);
        return true;
    }

    return false;
}

void UpdateClimbAnimationPlayback(Entity& player, const Graphics& graphics) {
    if (player.frame_data_animator.animation_id != frame_data_ids::PlayerClimbing) {
        return;
    }

    FrameDataAnimator& animator = player.frame_data_animator;
    animator.loop = true;
    animator.ResetSpeed();
    animator.finished = false;

    if (std::abs(player.vel.y) <= kClimbAnimationVelocityEpsilon) {
        animator.animate = false;
        return;
    }

    animator.animate = true;
    const AnimationPlaybackMode desired_mode =
        player.vel.y < 0.0F ? AnimationPlaybackMode::Forward : AnimationPlaybackMode::Reverse;
    if (animator.playback_mode == desired_mode) {
        return;
    }

    const FrameDataAnimation* const animation = graphics.frame_data_db.FindAnimation(animator.animation_id);
    if (animation != nullptr && animator.current_frame < animation->frame_indices.size()) {
        const FrameData& frame_data =
            graphics.frame_data_db.frames[animation->frame_indices[animator.current_frame]];
        const float frame_duration = static_cast<float>(frame_data.duration);
        animator.current_time = std::clamp(frame_duration - animator.current_time, 0.0F, frame_duration);
    }

    animator.playback_mode = desired_mode;
    animator.playback_dirty = false;
}

void StepPlayerFallTimer(Entity& player, const State& state) {
    const float fall_timer_rate =
        GetModifiedEffectValue(player, EffectModifierTarget::FallTimerRate, 1.0F, &state);
    if (fall_timer_rate <= 0.0F) {
        player.fall_timer = 0;
        return;
    }
    if (player.vel.y > 0.0F && !player.IsClimbing() && !player.IsHanging()) {
        if (fall_timer_rate >= 1.0F) {
            player.fall_timer += static_cast<std::uint32_t>(std::round(fall_timer_rate));
            return;
        }
        const std::uint32_t interval =
            static_cast<std::uint32_t>(std::max(1.0F, std::round(1.0F / fall_timer_rate)));
        if ((state.stage_frame % interval) == 0) {
            player.fall_timer += 1;
        }
        return;
    }

    player.fall_timer = 0;
}

float GetFallDamageTimer(const Entity& player, const State& state) {
    (void)state;
    return static_cast<float>(player.fall_timer);
}

unsigned int GetFallDamageAmount(
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

void SpawnFallDamagePoofs(const Entity& player, State& state) {
    const Vec2 base_pos = player.GetCenter() + Vec2::New(0.0F, player.size.y * 0.5F);
    for (float direction : {-1.0F, 1.0F}) {
        SpriteParticle smoke{};
        smoke.frame_data_animator = FrameDataAnimator::New(frame_data_ids::LittleSmoke);
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
    std::size_t entity_idx,
    State& state,
    Audio& audio,
    const PlayerPhysicsTuning& tuning
) {
    Entity& player = state.entity_manager.entities[entity_idx];
    if (!player.grounded) {
        return;
    }

    const float fall_damage_timer = GetFallDamageTimer(player, state);
    const bool was_holding_player =
        player.holding_vid.has_value() &&
        state.players.FindByEntityVid(*player.holding_vid) != nullptr;
    player.fall_timer = 0;
    if (fall_damage_timer <= static_cast<float>(tuning.fall_damage_min_frames) ||
        player.condition == EntityCondition::Dead) {
        return;
    }

    const unsigned int damage_amount = GetFallDamageAmount(tuning, fall_damage_timer);
    const common::DamageResult damage_result =
        common::TryDamageEntity(entity_idx, state, audio, DamageType::Fall, damage_amount);
    if (damage_result == common::DamageResult::None) {
        return;
    }

    Entity& mutable_player = state.entity_manager.entities[entity_idx];
    if (was_holding_player) {
        mutable_player.vel.y = 0.0F;
        mutable_player.grounded = true;
    } else {
        mutable_player.vel.y = kFallDamageBounceVelocityY;
        mutable_player.grounded = false;
    }
    SpawnFallDamagePoofs(mutable_player, state);
    (void)PlayEntityCenterSoundEmitter(state, mutable_player, audio_asset_ids::Thud);
}

void ControlEntityAsPlayerWithTuning(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt,
    const PlayerControlTuning& tuning
) {
    (void)graphics;
    (void)audio;
    (void)dt;
    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    Entity& player = state.entity_manager.entities[entity_idx];
    const controls::ControlIntent intent = controls::GetControlIntentForEntity(player, state);
    if (player.condition != EntityCondition::Normal) {
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

void ControlEntityAsPlayer(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    ControlEntityAsPlayerWithTuning(
        entity_idx,
        state,
        graphics,
        audio,
        dt,
        MakePlayerControlTuning(state.player_tuning)
    );
}

extern const EntityArchetype kPlayerArchetype{
    .type_ = EntityType::Player,
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
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Vulnerable,
    .damage_animation = frame_data_ids::BloodBall,
    .damage_sound = audio_asset_ids::PlayerOuch,
    .control_logic = ControlEntityAsPlayer,
    .step_logic = StepEntityLogicAsPlayer,
    .step_physics = StepEntityPhysicsAsPlayer,
    .alignment = Alignment::Ally,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::PlayerStanding),
};

void StepEntityLogicAsPlayer(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)dt;
    {
        // SKIP CONDITIONS
        Entity& player = state.entity_manager.entities[entity_idx];
        const EntityCondition player_condition = player.condition;
        if (player_condition == EntityCondition::Dead) {
            (void)common::SeverEntityCarryLinksForReset(player, state);
            gear_items::ClearEquippedPassiveItemVisuals(player, state, graphics);

            return;
        }
    }

    //  REQUIRED HACK FOR JETPACK TO CATCH GROUND TOUCH ON FRAME JUMP
    {
        Entity& player = state.entity_manager.entities[entity_idx];
        player.jumped_this_frame = false;
    }

    // TODO: probably put a check for dead or stunned up here lol
    common::StepTravelSoundWalkerClimber(entity_idx, state, audio);
    common::CleanupInactiveCarryReferences(entity_idx, state);

    {
        const Entity& player = state.entity_manager.entities[entity_idx];
        meathead::MaybePreviewMeatheadPassive(player, state);
    }

    const bool loss_of_control =
        state.entity_manager.entities[entity_idx].condition == EntityCondition::Stunned;
    const controls::ControlIntent control =
        controls::GetControlIntentForEntity(
            state.entity_manager.entities[entity_idx],
            state
        );

    {
        Entity& player = state.entity_manager.entities[entity_idx];
        const bool hanging = player.IsHanging();
        const bool climbing = player.IsClimbing();
        const bool walking =
            !loss_of_control &&
            (control.left != control.right) &&
            player.grounded &&
            !climbing &&
            !hanging;
        SetMovementFlag(player, EntityMovementFlag::Walking, walking);
        SetMovementFlag(player, EntityMovementFlag::Running, walking && control.run);
        SetMovementFlag(player, EntityMovementFlag::Climbing, climbing);
        SetMovementFlag(player, EntityMovementFlag::Hanging, hanging);
    }

    // SET ANIMATIONS AND DISPLAY STATES
    {
        Entity& player = state.entity_manager.entities[entity_idx];
        if (player.vel.x < 0.0F) {
            player.facing = LeftOrRight::Left;
        }
        if (player.vel.x > 0.0F) {
            player.facing = LeftOrRight::Right;
        }

        // skip all actions
        if (!loss_of_control) {
            // Hanging and climbing must win before locomotion. Otherwise walk/neutral
            // gets assigned first and the climb animation restarts every tick.
            if (player.hang_side == LeftOrRight::Left) {
                TrySetAnimation(player, EntityDisplayState::Hanging);
                player.facing = LeftOrRight::Left;
            } else if (player.hang_side == LeftOrRight::Right) {
                TrySetAnimation(player, EntityDisplayState::Hanging);
                player.facing = LeftOrRight::Right;
            } else if (player.IsClimbing()) {
                if (player.frame_data_animator.animation_id != frame_data_ids::PlayerClimbing) {
                    player.frame_data_animator.PlayLoop(frame_data_ids::PlayerClimbing);
                }
                player.frame_data_animator.loop = true;
                player.frame_data_animator.finished = false;
            } else if (TryStepPlayerEmote(player, control)) {
                player.frame_data_animator.ResetSpeed();
            } else {
                const bool holding_or_pushing =
                    player.holding ||
                    player.holding_vid.has_value() ||
                    HasMovementFlag(player, EntityMovementFlag::Pushing);
                const bool has_horizontal_input = control.left != control.right;
                if (has_horizontal_input) {
                    player.facing = control.left ? LeftOrRight::Left : LeftOrRight::Right;
                }
                const bool moving_with_input =
                    (control.left && player.vel.x < -kPlayerWalkAnimVelocityEpsilon) ||
                    (control.right && player.vel.x > kPlayerWalkAnimVelocityEpsilon);
                const bool walking_horizontally =
                    player.grounded &&
                    moving_with_input &&
                    player.dist_traveled_this_frame > kPlayerWalkAnimDistanceEpsilon;
                const bool running_horizontally = walking_horizontally && control.run;
                const bool pushing_into_blocker =
                    player.grounded &&
                    has_horizontal_input &&
                    !walking_horizontally;

                if (!player.grounded && player.vel.y > 0.0F) {
                    TrySetAnimation(player, EntityDisplayState::Falling);
                    player.frame_data_animator.ResetSpeed();
                } else if (walking_horizontally) {
                    TrySetAnimation(
                        player,
                        holding_or_pushing ? EntityDisplayState::WalkHolding : EntityDisplayState::Walk
                    );
                    player.frame_data_animator.SetSpeed(
                        running_horizontally ? kPlayerRunAnimSpeed : kPlayerWalkAnimSpeed
                    );
                } else if (pushing_into_blocker) {
                    TrySetAnimation(player, EntityDisplayState::WalkHolding);
                    player.frame_data_animator.SetSpeed(kPlayerBlockedPushAnimSpeed);
                } else {
                    TrySetAnimation(
                        player,
                        holding_or_pushing ? EntityDisplayState::NeutralHolding : EntityDisplayState::Neutral
                    );
                    player.frame_data_animator.ResetSpeed();
                }
            }
        }
    }

    common::UpdateCarryAndBackItems(entity_idx, state, graphics, audio);

    // PLAYER TOOL SLOT 1
    if (!loss_of_control) {
        common::TryUseToolSlot(entity_idx, state, graphics, audio, 0, control.bomb_pressed);
    }

    // PLAYER TOOL SLOT 2
    if (!loss_of_control) {
        common::TryUseToolSlot(entity_idx, state, graphics, audio, 1, control.rope_pressed);
    }

    // PLAYER SWING BAT SECTION
    {
        Entity& player = state.entity_manager.entities[entity_idx];
        if (player.attack_delay_countdown > 0) {
            player.attack_delay_countdown -= 1;
        }
    }
    if (!loss_of_control) {
        const Entity& player = state.entity_manager.entities[entity_idx];
        const Vec2 player_pos = player.pos;
        const bool trying_to_attack = control.attack_pressed;
        const VID player_vid = player.vid;
        const unsigned int attack_delay_countdown = player.attack_delay_countdown;
        const bool has_held_item = player.holding_vid.has_value();

        bool attacked = false;
        if (trying_to_attack && attack_delay_countdown == 0 && !has_held_item) {
            if (world_ops::SpawnEntity(
                    state,
                    EntityType::BaseballBat,
                    [&](Entity& entity) {
                        entity.pos = player_pos;
                        entity.held_by_vid = player_vid;
                        entity.attachment_mode = AttachmentMode::Held;
                        state.UpdateSidForEntity(entity.vid.id, graphics);
                    },
                    player_vid
                ) != nullptr) {
                attacked = true;
                if (const Entity* const sound_player = state.entity_manager.GetEntity(player_vid)) {
                    (void)PlayEntitySoundEmitter(state, *sound_player, audio_asset_ids::BaseballBatSwing);
                }
            }
        }
        if (attacked) {
            Entity& mutable_player = state.entity_manager.entities[entity_idx];
            mutable_player.attack_delay_countdown = kAttackDelay;
        }
    }

    // PUSH BLOCKS
    if (!loss_of_control) {
        common::TryPushBlocks(entity_idx, state, graphics);
    }

}

namespace {

void StepEntityPhysicsAsPlayerWithTuning(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt,
    const PlayerPhysicsTuning& tuning
) {
    common::HangHandsStep(entity_idx, state, tuning.jump_and_climb);
    common::JumpingAndClimbingStep(entity_idx, state, audio, tuning.jump_and_climb);

    // custom pre partial euler step for player to apply special velocity clamping.
    Entity& entity = state.entity_manager.entities[entity_idx];
    const bool externally_launched_player = IsExternallyLaunchedPlayer(entity);
    const bool has_punish_ball = PlayerHasPunishBall(entity, state);
    const bool holding_punish_ball = PlayerIsHoldingPunishBall(entity, state);
    if (has_punish_ball && !holding_punish_ball && !entity.IsClimbing()) {
        entity.acc.y += kPunishBallDraggedExtraGravity;
    }

    if (entity.IsClimbing()) {
        UpdateClimbAnimationPlayback(entity, graphics);
    }

    entity.vel += entity.acc;
    if (has_punish_ball && !holding_punish_ball && entity.jumped_this_frame &&
        entity.vel.y < -kPunishBallDraggedJumpImpulse) {
        entity.vel.y = -kPunishBallDraggedJumpImpulse;
    }
    const controls::ControlIntent control =
        controls::GetControlIntentForEntity(entity, state);
    const float max_walk_speed =
        holding_punish_ball ? kPunishBallHeldMaxWalkSpeed
                            : (has_punish_ball ? kPunishBallDraggedMaxWalkSpeed : tuning.max_walk_speed);
    const float max_run_speed =
        holding_punish_ball ? kPunishBallHeldMaxRunSpeed
                            : (has_punish_ball ? kPunishBallDraggedMaxRunSpeed : tuning.max_run_speed);
    const float move_speed_scale = std::max(
        0.0F,
        GetModifiedEffectValue(entity, EffectModifierTarget::MoveSpeedScale, 1.0F, &state)
    );
    if (!externally_launched_player) {
        if (control.run) {
            entity.vel.x = std::clamp(entity.vel.x, -max_run_speed * move_speed_scale, max_run_speed * move_speed_scale);
        } else {
            entity.vel.x =
                std::clamp(entity.vel.x, -max_walk_speed * move_speed_scale, max_walk_speed * move_speed_scale);
        }
    }
    const float max_fall_speed =
        GetModifiedEffectValue(entity, EffectModifierTarget::MaxFallSpeed, tuning.max_speed, &state);
    entity.vel.y = std::min(entity.vel.y, max_fall_speed);
    StepPlayerFallTimer(entity, state);
    gear_items::StepEquippedPassiveItems(entity_idx, state, graphics);

    common::DoTileAndEntityCollisions(entity_idx, state, graphics, audio);
    common::ApplyArchetypeGroundFriction(entity_idx, state, tuning.ground_friction_scale);
    if (!entity.grounded && !externally_launched_player) {
        entity.vel.x *= tuning.air_friction;
    }
    ApplyClassicFallDamageOnLanding(entity_idx, state, audio, tuning);
    common::PostPartialEulerStep(entity_idx, state, dt);
}

} // namespace

/** generalize this to all square or rectangular entities somehow */
void StepEntityPhysicsAsPlayer(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    StepEntityPhysicsAsPlayerWithTuning(
        entity_idx,
        state,
        graphics,
        audio,
        dt,
        MakePlayerPhysicsTuning(state.player_tuning)
    );
}

} // namespace splonks::entities::player

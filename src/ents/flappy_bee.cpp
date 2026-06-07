#include "ents/flappy_bee.hpp"

#include "audio.hpp"
#include "audio_emitters.hpp"
#include "controls.hpp"
#include "ents/common/common.hpp"
#include "aframe_id.hpp"
#include "state.hpp"

#include <algorithm>
#include <cmath>

namespace splonks::ents::flappy_bee {

namespace {

constexpr float kGroundTargetSpeed = 0.55F;
constexpr float kAirTargetSpeed = 2.15F;
constexpr float kGroundMoveAcc = 0.035F;
constexpr float kAirMoveAcc = 0.11F;
constexpr float kFlapImpulse = 3.25F;
constexpr float kMaxFallSpeed = 5.0F;
constexpr float kMaxRiseSpeed = 4.25F;
constexpr float kMaxHorizontalSpeed = 2.35F;
constexpr float kAirNoInputDamping = 0.97F;
constexpr float kRotationDegreesPerYVelocity = 10.0F;
constexpr float kMinRotation = -32.0F;
constexpr float kMaxRotation = 65.0F;
constexpr float kWalkAnimVelocityEpsilon = 0.05F;
constexpr float kFlapSoundVolumeScale = 0.3F;

void SetBeeAnim(Ent& bee) {
    if (!bee.grounded) {
        SetAnim(bee, aframe_ids::BeeFly);
        bee.aframe_animator.animate = true;
        return;
    }

    SetAnim(bee, aframe_ids::BeeWalk);
    bee.aframe_animator.animate = std::abs(bee.vel.x) > kWalkAnimVelocityEpsilon;
}

void UpdateBeeRotation(Ent& bee) {
    if (bee.grounded) {
        bee.rotation = sim::Scalar::zero();
        return;
    }

    bee.rotation = sim::ToSimScalar(std::clamp(
        bee.vel.y * kRotationDegreesPerYVelocity,
        kMinRotation,
        kMaxRotation
    ));
}

} // namespace

extern const EntSpec kFlappyBeeSpec{
    .type_ = EntType::FlappyBee,
    .size = Vec2::New(8.0F, 8.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .can_collect_pickups = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_stomp = false,
    .can_hang_ledge = false,
    .can_be_stunned = true,
    .stun_recovers_on_ground = true,
    .stun_recovers_while_held = false,
    .throw_velocity_scale = sim::ToSimScalar(0.1F),
    .draw_layer = DrawLayer::Middle,
    .facing = Side::Right,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::Vulnerable,
    .damage_anim = aframe_ids::BloodBall,
    .damage_sound = audio_asset_ids::PlayerOuch,
    .death_sound = audio_asset_ids::BeeSplat,
    .on_death = OnDeathAsFlappyBee,
    .control_logic = ControlEntAsFlappyBee,
    .step_logic = StepEntLogicAsFlappyBee,
    .step_physics = StepEntPhysicsAsFlappyBee,
    .alignment = Alignment::Ally,
    .aframe_animator = AFrameAnimator::New(aframe_ids::BeeFly),
};

void OnDeathAsFlappyBee(std::size_t ent_idx, State& state, Audio& audio) {
    (void)audio;
    if (ent_idx >= state.ents.ents.size()) {
        return;
    }

    Ent& bee = state.ents.ents[ent_idx];
    bee.render_enabled = false;
    bee.has_physics = false;
    bee.can_collide = false;
    bee.vel = Vec2::New(0.0F, 0.0F);
    bee.acc = Vec2::New(0.0F, 0.0F);
}

void ControlEntAsFlappyBee(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    (void)dt;
    if (ent_idx >= state.ents.ents.size()) {
        return;
    }

    Ent& bee = state.ents.ents[ent_idx];
    const controls::ControlIntent control = controls::GetControlIntentForEnt(bee, state);
    if (bee.condition != EntCondition::Normal) {
        return;
    }

    if (control.left && !control.right) {
        const float target_speed = bee.grounded ? -kGroundTargetSpeed : -kAirTargetSpeed;
        const float acc = bee.grounded ? kGroundMoveAcc : kAirMoveAcc;
        common::AccelerateHorizontallyTowardSpeed(bee, state, target_speed, acc);
        bee.facing = Side::Left;
    } else if (control.right && !control.left) {
        const float target_speed = bee.grounded ? kGroundTargetSpeed : kAirTargetSpeed;
        const float acc = bee.grounded ? kGroundMoveAcc : kAirMoveAcc;
        common::AccelerateHorizontallyTowardSpeed(bee, state, target_speed, acc);
        bee.facing = Side::Right;
    } else if (!bee.grounded) {
        bee.vel.x *= kAirNoInputDamping;
    }

    if (control.jump_pressed) {
        bee.vel.y = -kFlapImpulse;
        bee.grounded = false;
        AudioEmitterPlayParams params;
        params.volume_scale = kFlapSoundVolumeScale;
        (void)PlayEntCenterSoundEmitter(state, bee, audio_asset_ids::Buzz, params);
    }

    if (control.stop) {
        bee.acc = Vec2::New(0.0F, 0.0F);
        bee.vel = Vec2::New(0.0F, 0.0F);
    }
}

void StepEntLogicAsFlappyBee(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)dt;
    if (ent_idx >= state.ents.ents.size()) {
        return;
    }

    Ent& bee = state.ents.ents[ent_idx];
    if (bee.condition == EntCondition::Dead) {
        return;
    }

    common::CleanupInactiveCarryReferences(ent_idx, state);

    const bool loss_of_control = bee.condition == EntCondition::Stunned;
    const controls::ControlIntent control = controls::GetControlIntentForEnt(bee, state);

    const bool walking =
        !loss_of_control &&
        bee.grounded &&
        (control.left != control.right) &&
        std::abs(bee.vel.x) > kWalkAnimVelocityEpsilon;
    SetMovementFlag(bee, EntMovementFlag::Walking, walking);
    SetMovementFlag(bee, EntMovementFlag::Running, false);
    SetMovementFlag(bee, EntMovementFlag::Climbing, false);
    SetMovementFlag(bee, EntMovementFlag::Hanging, false);

    SetBeeAnim(bee);
    common::UpdateCarryAndBackItems(ent_idx, state, graphics, audio);

    if (!loss_of_control) {
        common::TryUseToolSlot(ent_idx, state, graphics, audio, 0, control.bomb_pressed);
        common::TryUseToolSlot(ent_idx, state, graphics, audio, 1, control.rope_pressed);
    }
}

void StepEntPhysicsAsFlappyBee(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    common::ApplyGravity(ent_idx, state, dt);
    common::PrePartialEulerStep(ent_idx, state, dt);

    Ent& bee = state.ents.ents[ent_idx];
    bee.vel.x = std::clamp(bee.vel.x, -kMaxHorizontalSpeed, kMaxHorizontalSpeed);
    bee.vel.y = std::clamp(bee.vel.y, -kMaxRiseSpeed, kMaxFallSpeed);

    common::DoTileAndEntCollisions(ent_idx, state, graphics, audio);
    common::ApplySpecGroundFriction(ent_idx, state);
    UpdateBeeRotation(bee);
    common::PostPartialEulerStep(ent_idx, state, dt);
}

} // namespace splonks::ents::flappy_bee

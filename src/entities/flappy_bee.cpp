#include "entities/flappy_bee.hpp"

#include "audio.hpp"
#include "audio_emitters.hpp"
#include "controls.hpp"
#include "entities/common/common.hpp"
#include "frame_data_id.hpp"
#include "state.hpp"

#include <algorithm>
#include <cmath>

namespace splonks::entities::flappy_bee {

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
constexpr float kWalkAnimationVelocityEpsilon = 0.05F;
constexpr float kFlapSoundVolumeScale = 0.3F;

void SetBeeAnimation(Entity& bee) {
    if (!bee.grounded) {
        SetAnimation(bee, frame_data_ids::BeeFly);
        bee.frame_data_animator.animate = true;
        return;
    }

    SetAnimation(bee, frame_data_ids::BeeWalk);
    bee.frame_data_animator.animate = std::abs(bee.vel.x) > kWalkAnimationVelocityEpsilon;
}

void UpdateBeeRotation(Entity& bee) {
    if (bee.grounded) {
        bee.rotation = 0.0F;
        return;
    }

    bee.rotation = std::clamp(
        bee.vel.y * kRotationDegreesPerYVelocity,
        kMinRotation,
        kMaxRotation
    );
}

} // namespace

extern const EntityArchetype kFlappyBeeArchetype{
    .type_ = EntityType::FlappyBee,
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
    .throw_velocity_scale = 0.1F,
    .draw_layer = DrawLayer::Middle,
    .facing = LeftOrRight::Right,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Vulnerable,
    .damage_animation = frame_data_ids::BloodBall,
    .damage_sound = audio_asset_ids::PlayerOuch,
    .death_sound = audio_asset_ids::BeeSplat,
    .on_death = OnDeathAsFlappyBee,
    .control_logic = ControlEntityAsFlappyBee,
    .step_logic = StepEntityLogicAsFlappyBee,
    .step_physics = StepEntityPhysicsAsFlappyBee,
    .alignment = Alignment::Ally,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::BeeFly),
};

void OnDeathAsFlappyBee(std::size_t entity_idx, State& state, Audio& audio) {
    (void)audio;
    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    Entity& bee = state.entity_manager.entities[entity_idx];
    bee.render_enabled = false;
    bee.has_physics = false;
    bee.can_collide = false;
    bee.vel = Vec2::New(0.0F, 0.0F);
    bee.acc = Vec2::New(0.0F, 0.0F);
}

void ControlEntityAsFlappyBee(
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

    Entity& bee = state.entity_manager.entities[entity_idx];
    const controls::ControlIntent control = controls::GetControlIntentForEntity(bee, state);
    if (bee.condition != EntityCondition::Normal) {
        return;
    }

    if (control.left && !control.right) {
        const float target_speed = bee.grounded ? -kGroundTargetSpeed : -kAirTargetSpeed;
        const float acc = bee.grounded ? kGroundMoveAcc : kAirMoveAcc;
        common::AccelerateHorizontallyTowardSpeed(bee, target_speed, acc);
        bee.facing = LeftOrRight::Left;
    } else if (control.right && !control.left) {
        const float target_speed = bee.grounded ? kGroundTargetSpeed : kAirTargetSpeed;
        const float acc = bee.grounded ? kGroundMoveAcc : kAirMoveAcc;
        common::AccelerateHorizontallyTowardSpeed(bee, target_speed, acc);
        bee.facing = LeftOrRight::Right;
    } else if (!bee.grounded) {
        bee.vel.x *= kAirNoInputDamping;
    }

    if (control.jump_pressed) {
        bee.vel.y = -kFlapImpulse;
        bee.grounded = false;
        AudioEmitterPlayParams params;
        params.volume_scale = kFlapSoundVolumeScale;
        (void)PlayEntityCenterSoundEmitter(state, bee, audio_asset_ids::Buzz, params);
    }

    if (control.stop) {
        bee.acc = Vec2::New(0.0F, 0.0F);
        bee.vel = Vec2::New(0.0F, 0.0F);
    }
}

void StepEntityLogicAsFlappyBee(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)dt;
    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    Entity& bee = state.entity_manager.entities[entity_idx];
    if (bee.condition == EntityCondition::Dead) {
        return;
    }

    common::CleanupInactiveCarryReferences(entity_idx, state);

    const bool loss_of_control = bee.condition == EntityCondition::Stunned;
    const controls::ControlIntent control = controls::GetControlIntentForEntity(bee, state);

    const bool walking =
        !loss_of_control &&
        bee.grounded &&
        (control.left != control.right) &&
        std::abs(bee.vel.x) > kWalkAnimationVelocityEpsilon;
    SetMovementFlag(bee, EntityMovementFlag::Walking, walking);
    SetMovementFlag(bee, EntityMovementFlag::Running, false);
    SetMovementFlag(bee, EntityMovementFlag::Climbing, false);
    SetMovementFlag(bee, EntityMovementFlag::Hanging, false);

    SetBeeAnimation(bee);
    common::UpdateCarryAndBackItems(entity_idx, state, graphics, audio);

    if (!loss_of_control) {
        common::TryUseToolSlot(entity_idx, state, graphics, audio, 0, control.bomb_pressed);
        common::TryUseToolSlot(entity_idx, state, graphics, audio, 1, control.rope_pressed);
    }
}

void StepEntityPhysicsAsFlappyBee(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    common::ApplyGravity(entity_idx, state, dt);
    common::PrePartialEulerStep(entity_idx, state, dt);

    Entity& bee = state.entity_manager.entities[entity_idx];
    bee.vel.x = std::clamp(bee.vel.x, -kMaxHorizontalSpeed, kMaxHorizontalSpeed);
    bee.vel.y = std::clamp(bee.vel.y, -kMaxRiseSpeed, kMaxFallSpeed);

    common::DoTileAndEntityCollisions(entity_idx, state, graphics, audio);
    common::ApplyArchetypeGroundFriction(entity_idx, state);
    UpdateBeeRotation(bee);
    common::PostPartialEulerStep(entity_idx, state, dt);
}

} // namespace splonks::entities::flappy_bee

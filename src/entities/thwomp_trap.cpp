#include "entities/thwomp_trap.hpp"

#include "audio.hpp"
#include "entities/block.hpp"
#include "entities/common/common.hpp"
#include "frame_data_id.hpp"
#include "state.hpp"
#include "world_query.hpp"

#include <algorithm>
#include <cmath>

namespace splonks::entities::thwomp_trap {

namespace {

constexpr float kTriggerDistance = 96.0F;
constexpr float kTriggerHalfWidth = 8.0F;
constexpr float kDropGravity = 0.45F;
constexpr float kDropMaxVelocity = 6.0F;
constexpr float kReturnVelocity = -1.0F;
constexpr float kWaitFrames = 100.0F;
constexpr float kImpactShake = 0.45F;
constexpr float kImpactTileShake = 0.36F;
constexpr float kImpactShakeRadiusTiles = 1.2F;

bool HasHomePosition(const Entity& thwomp) {
    return thwomp.point_label_a == PointLabel::Target;
}

void StoreHomePosition(Entity& thwomp) {
    if (HasHomePosition(thwomp)) {
        return;
    }
    thwomp.point_label_a = PointLabel::Target;
    thwomp.point_a = ToIVec2(thwomp.pos);
}

float GetHomeY(const Entity& thwomp) {
    return static_cast<float>(thwomp.point_a.y);
}

bool IsDropping(const Entity& thwomp) {
    return thwomp.ai_state == EntityAiState::Disturbed;
}

bool IsWaiting(const Entity& thwomp) {
    return thwomp.ai_state == EntityAiState::Pursuing;
}

bool IsReturning(const Entity& thwomp) {
    return thwomp.ai_state == EntityAiState::Returning;
}

bool ShouldDrop(const Entity& thwomp, const State& state) {
    const Vec2 thwomp_center = thwomp.GetCenter();
    for (const Entity& entity : state.entity_manager.entities) {
        if (!entity.active || !IsPlayerLikeEntityType(entity.type_) ||
            entity.condition == EntityCondition::Dead) {
            continue;
        }

        const Vec2 delta = GetNearestWorldDelta(state.stage, thwomp_center, entity.GetCenter());
        if (delta.y <= 0.0F || delta.y > kTriggerDistance ||
            std::abs(delta.x) > kTriggerHalfWidth) {
            continue;
        }
        return true;
    }
    return false;
}

void StartDrop(Entity& thwomp) {
    thwomp.ai_state = EntityAiState::Disturbed;
    thwomp.vel = Vec2::New(0.0F, 0.0F);
    thwomp.acc = Vec2::New(0.0F, 0.0F);
}

void StartWait(Entity& thwomp, State& state) {
    thwomp.ai_state = EntityAiState::Pursuing;
    thwomp.vel = Vec2::New(0.0F, 0.0F);
    thwomp.acc = Vec2::New(0.0F, 0.0F);
    thwomp.counter_a = kWaitFrames;
    AddEntityShake(thwomp, kImpactShake);
    AddShake(
        state,
        thwomp.GetCenter(),
        kImpactTileShake,
        kImpactTileShake * 0.65F,
        0.0F,
        kImpactShakeRadiusTiles,
        thwomp.vid
    );
    (void)PlayEntityCenterSoundEmitter(state, thwomp, audio_asset_ids::BoulderHitGround);
}

void StartReturn(Entity& thwomp) {
    thwomp.ai_state = EntityAiState::Returning;
    thwomp.vel = Vec2::New(0.0F, kReturnVelocity);
    thwomp.acc = Vec2::New(0.0F, 0.0F);
}

void FinishReturn(Entity& thwomp) {
    thwomp.ai_state = EntityAiState::Idle;
    thwomp.pos.y = GetHomeY(thwomp);
    thwomp.vel = Vec2::New(0.0F, 0.0F);
    thwomp.acc = Vec2::New(0.0F, 0.0F);
    thwomp.grounded = false;
    thwomp.collided = false;
}

} // namespace

void StepEntityLogicAsThwompTrap(
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

    Entity& thwomp = state.entity_manager.entities[entity_idx];
    SetAnimation(thwomp, frame_data_ids::ThwompTrap);
    StoreHomePosition(thwomp);

    if (thwomp.condition == EntityCondition::Dead) {
        return;
    }

    if (thwomp.ai_state == EntityAiState::Idle) {
        thwomp.vel = Vec2::New(0.0F, 0.0F);
        thwomp.acc = Vec2::New(0.0F, 0.0F);
        if (ShouldDrop(thwomp, state)) {
            StartDrop(thwomp);
        }
        return;
    }

    if (IsWaiting(thwomp)) {
        thwomp.vel = Vec2::New(0.0F, 0.0F);
        thwomp.acc = Vec2::New(0.0F, 0.0F);
        thwomp.counter_a -= 1.0F;
        if (thwomp.counter_a <= 0.0F) {
            StartReturn(thwomp);
        }
    }
}

void StepEntityPhysicsAsThwompTrap(
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

    Entity& thwomp = state.entity_manager.entities[entity_idx];
    if (!IsDropping(thwomp) && !IsReturning(thwomp)) {
        thwomp.collided_last_frame = thwomp.collided;
        thwomp.collided = false;
        thwomp.grounded = false;
        return;
    }

    const bool was_grounded = thwomp.grounded;
    const float pre_vel_y = thwomp.vel.y;

    if (IsDropping(thwomp)) {
        thwomp.acc.y += kDropGravity;
    } else {
        thwomp.vel = Vec2::New(0.0F, kReturnVelocity);
        thwomp.acc = Vec2::New(0.0F, 0.0F);
    }

    common::PrePartialEulerStep(entity_idx, state, dt);
    if (IsDropping(thwomp)) {
        thwomp.vel.y = std::clamp(thwomp.vel.y, 0.0F, kDropMaxVelocity);
    }
    common::DoTileAndEntityCollisions(entity_idx, state, graphics, audio);
    common::PostPartialEulerStep(entity_idx, state, dt);

    if (IsDropping(thwomp)) {
        const bool hit_bottom = (!was_grounded && thwomp.grounded) ||
                                (pre_vel_y > 0.0F && thwomp.collided && thwomp.vel.y == 0.0F);
        if (hit_bottom) {
            StartWait(thwomp, state);
        }
        return;
    }

    if (IsReturning(thwomp)) {
        if (thwomp.pos.y <= GetHomeY(thwomp) ||
            (pre_vel_y < 0.0F && thwomp.collided && thwomp.vel.y == 0.0F)) {
            FinishReturn(thwomp);
        }
    }
}

extern const EntityArchetype kThwompTrapArchetype{
    .type_ = EntityType::ThwompTrap,
    .size = Vec2::New(static_cast<float>(kTileSize), static_cast<float>(kTileSize)),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_hit = true,
    .can_receive_projectile_contact = true,
    .can_be_picked_up = false,
    .impassable = true,
    .hurt_on_contact = false,
    .crusher_pusher = true,
    .can_be_stomped = false,
    .vanish_on_death = true,
    .can_be_stunned = false,
    .affected_by_ground_friction = false,
    .draw_layer = DrawLayer::Middle,
    .condition = EntityCondition::Normal,
    .ai_state = EntityAiState::Idle,
    .damage_vulnerability = DamageVulnerability::ExplosionOnly,
    .projectile_contact_damage_amount = 0,
    .on_death = block::OnDeathAsBlock,
    .step_logic = StepEntityLogicAsThwompTrap,
    .step_physics = StepEntityPhysicsAsThwompTrap,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::ThwompTrap),
};

} // namespace splonks::entities::thwomp_trap

#include "entities/mantrap.hpp"

#include "audio.hpp"
#include "entity/archetype.hpp"
#include "entities/common/common.hpp"
#include "entities/common/ground_walker.hpp"
#include "frame_data_animator.hpp"
#include "frame_data_id.hpp"
#include "state.hpp"
#include "world_query.hpp"

#include <cmath>

namespace splonks::entities::mantrap {

namespace {

constexpr float kMantrapWalkSpeed = 1.0F;
constexpr float kMantrapWalkAcceleration = 0.2F;
constexpr int kMantrapIdleMinFrames = 20;
constexpr int kMantrapIdleMaxFrames = 50;
constexpr int kMantrapIdleChance = 100;
constexpr float kMantrapEatFrames = 54.0F;
constexpr unsigned int kMantrapEatDamage = 9999;

void StartIdle(Entity& mantrap) {
    mantrap.ai_state = EntityAiState::Idle;
    mantrap.counter_a = static_cast<float>(rng::RandomIntInclusive(kMantrapIdleMinFrames, kMantrapIdleMaxFrames));
    common::DecelerateHorizontallyToStop(mantrap, kMantrapWalkAcceleration);
    TrySetAnimation(mantrap, EntityDisplayState::Neutral);
}

void StartWalking(Entity& mantrap) {
    mantrap.ai_state = EntityAiState::Patrolling;
    common::AccelerateHorizontallyTowardSpeed(
        mantrap,
        mantrap.facing == LeftOrRight::Left ? -kMantrapWalkSpeed : kMantrapWalkSpeed,
        kMantrapWalkAcceleration
    );
    TrySetAnimation(mantrap, EntityDisplayState::Walk);
}

bool CanMantrapEatEntity(const Entity& target) {
    if (!target.active || !target.can_collide || target.condition == EntityCondition::Dead) {
        return false;
    }

    return IsPlayerLikeEntityType(target.type_) ||
           target.type_ == EntityType::Damsel ||
           target.type_ == EntityType::Caveman ||
           target.type_ == EntityType::Shopkeeper;
}

void FaceTarget(Entity& mantrap, const Entity& target, const Stage& stage) {
    const Vec2 delta = GetNearestWorldDelta(stage, mantrap.GetCenter(), target.GetCenter());
    if (delta.x < 0.0F) {
        mantrap.facing = LeftOrRight::Left;
    } else if (delta.x > 0.0F) {
        mantrap.facing = LeftOrRight::Right;
    }
}

bool TryEatOverlappingEntity(
    std::size_t mantrap_idx,
    State& state,
    Graphics& graphics,
    Audio& audio
) {
    Entity& mantrap = state.entity_manager.entities[mantrap_idx];
    const AABB mantrap_aabb = common::GetContactAabbForEntity(mantrap, graphics);
    for (const VID& target_vid : QueryEntitiesInAabb(state, mantrap_aabb, mantrap.vid)) {
        Entity* const target = state.entity_manager.GetEntityMut(target_vid);
        if (target == nullptr || !CanMantrapEatEntity(*target)) {
            continue;
        }
        const AABB target_aabb = GetNearestWorldAabb(
            state.stage,
            mantrap.GetCenter(),
            common::GetContactAabbForEntity(*target, graphics)
        );
        if (!AabbsIntersect(mantrap_aabb, target_aabb)) {
            continue;
        }

        FaceTarget(mantrap, *target, state.stage);
        mantrap.vel.x = 0.0F;
        mantrap.counter_b = kMantrapEatFrames;
        SetAnimation(mantrap, frame_data_ids::MantrapEat);
        const common::DamageResult damage_result =
            common::TryDamageEntity(target_vid.id, state, audio, DamageType::Attack, kMantrapEatDamage);
        if (damage_result == common::DamageResult::Died && target->active &&
            !IsPlayerLikeEntityType(target->type_)) {
            target->marked_for_destruction = true;
        }
        (void)PlayEntityCenterSoundEmitter(state, mantrap, audio_asset_ids::AnimalCrush1);
        return true;
    }
    return false;
}

} // namespace

void StepEntityLogicAsMantrap(
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

    Entity& mantrap = state.entity_manager.entities[entity_idx];
    if (mantrap.last_condition == EntityCondition::Stunned &&
        mantrap.condition == EntityCondition::Normal) {
        StartIdle(mantrap);
    }
    if (mantrap.condition != EntityCondition::Normal) {
        return;
    }

    if (mantrap.counter_b > 0.0F) {
        mantrap.counter_b -= 1.0F;
        mantrap.vel.x = 0.0F;
        SetAnimation(mantrap, frame_data_ids::MantrapEat);
        return;
    }

    if (TryEatOverlappingEntity(entity_idx, state, graphics, audio)) {
        return;
    }

    if (mantrap.ai_state == EntityAiState::Idle) {
        common::DecelerateHorizontallyToStop(mantrap, kMantrapWalkAcceleration);
        TrySetAnimation(mantrap, EntityDisplayState::Neutral);
        if (mantrap.counter_a > 0.0F) {
            mantrap.counter_a -= 1.0F;
            return;
        }

        mantrap.facing = rng::RandomIntInclusive(0, 1) == 0 ? LeftOrRight::Left : LeftOrRight::Right;
        StartWalking(mantrap);
        return;
    }

    int direction = mantrap.facing == LeftOrRight::Left ? -1 : 1;
    if (common::HasWallAheadForGroundWalker(mantrap, state, graphics, direction) ||
        !common::HasGroundAheadForGroundWalker(mantrap, state, graphics, direction)) {
        mantrap.facing = mantrap.facing == LeftOrRight::Left ? LeftOrRight::Right : LeftOrRight::Left;
        direction = -direction;
    }

    if (rng::RandomIntInclusive(1, kMantrapIdleChance) == 1) {
        StartIdle(mantrap);
        return;
    }

    common::AccelerateHorizontallyTowardSpeed(
        mantrap,
        static_cast<float>(direction) * kMantrapWalkSpeed,
        kMantrapWalkAcceleration
    );
    SetMovementFlag(mantrap, EntityMovementFlag::Walking, true);
    TrySetAnimation(mantrap, EntityDisplayState::Walk);
}

extern const EntityArchetype kMantrapArchetype{
    .type_ = EntityType::Mantrap,
    .size = Vec2::New(9.0F, 13.0F),
    .health = 3,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .can_only_be_picked_up_if_dead_or_stunned = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = true,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .ai_state = EntityAiState::Idle,
    .display_state = EntityDisplayState::Neutral,
    .counter_a = static_cast<float>(kMantrapIdleMinFrames),
    .damage_vulnerability = DamageVulnerability::Vulnerable,
    .damage_animation = frame_data_ids::BloodBall,
    .damage_sound = audio_asset_ids::CavemanHurt,
    .collide_sound = audio_asset_ids::Thud,
    .step_logic = StepEntityLogicAsMantrap,
    .alignment = Alignment::Enemy,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::Mantrap),
};

} // namespace splonks::entities::mantrap

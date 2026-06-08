#include "ents/mantrap.hpp"

#include "audio.hpp"
#include "ent/spec.hpp"
#include "ents/common/common.hpp"
#include "ents/common/ground_walker.hpp"
#include "aframe_animator.hpp"
#include "aframe_id.hpp"
#include "state.hpp"
#include "world_query.hpp"

#include <cmath>

namespace splonks::ents::mantrap {

namespace {

constexpr float kMantrapWalkSpeed = 1.0F;
constexpr float kMantrapWalkAcceleration = 0.2F;
constexpr int kMantrapIdleMinFrames = 20;
constexpr int kMantrapIdleMaxFrames = 50;
constexpr int kMantrapIdleChance = 100;
constexpr float kMantrapEatFrames = 54.0F;
constexpr std::uint32_t kMantrapEatDamage = 9999;

void StartIdle(Ent& mantrap, State& state) {
    mantrap.ai_state = EntAiState::Idle;
    mantrap.counter_a = static_cast<float>(
        state.drng.RandomIntInclusive(kMantrapIdleMinFrames, kMantrapIdleMaxFrames));
    common::DecelerateHorizontallyToStop(mantrap, kMantrapWalkAcceleration);
    TrySetAnim(mantrap, EntDisplayState::Neutral);
}

void StartWalking(Ent& mantrap, const State& state) {
    mantrap.ai_state = EntAiState::Patrolling;
    common::AccelerateHorizontallyTowardSpeed(
        mantrap,
        state,
        mantrap.facing == Side::Left ? -kMantrapWalkSpeed : kMantrapWalkSpeed,
        kMantrapWalkAcceleration
    );
    TrySetAnim(mantrap, EntDisplayState::Walk);
}

bool CanMantrapEatEnt(const Ent& target) {
    if (!target.active || !target.can_collide || target.condition == EntCondition::Dead) {
        return false;
    }

    return IsPlayerLikeEntType(target.type_) ||
           target.type_ == EntType::Damsel ||
           target.type_ == EntType::Caveman ||
           target.type_ == EntType::Shopkeeper;
}

void FaceTarget(Ent& mantrap, const Ent& target, const Stage& stage) {
    const Vec2 delta = GetNearestWorldDelta(stage, mantrap.GetCenter(), target.GetCenter());
    if (delta.x < 0.0F) {
        mantrap.facing = Side::Left;
    } else if (delta.x > 0.0F) {
        mantrap.facing = Side::Right;
    }
}

bool TryEatOverlappingEnt(
    std::size_t mantrap_idx,
    State& state,
    Graphics& graphics,
    Audio& audio
) {
    Ent& mantrap = state.ents.ents[mantrap_idx];
    const AABB mantrap_aabb = common::GetContactAabbForEnt(mantrap, graphics);
    for (const VID& target_vid : QueryEntsInAabb(state, mantrap_aabb, mantrap.vid)) {
        Ent* const target = state.ents.GetEntMut(target_vid);
        if (target == nullptr || !CanMantrapEatEnt(*target)) {
            continue;
        }
        const AABB target_aabb = GetNearestWorldAabb(
            state.stage,
            mantrap.GetCenter(),
            common::GetContactAabbForEnt(*target, graphics)
        );
        if (!AabbsIntersect(mantrap_aabb, target_aabb)) {
            continue;
        }

        FaceTarget(mantrap, *target, state.stage);
        mantrap.vel.x = 0.0F;
        mantrap.counter_b = kMantrapEatFrames;
        SetAnim(mantrap, aframe_ids::MantrapEat);
        const common::DamageResult damage_result =
            common::TryDamageEnt(target_vid.id, state, audio, DamageType::Attack, kMantrapEatDamage);
        if (damage_result == common::DamageResult::Died && target->active &&
            !IsPlayerLikeEntType(target->type_)) {
            target->marked_for_destruction = true;
        }
        (void)PlayEntCenterSoundEmitter(state, mantrap, audio_asset_ids::AnimalCrush1);
        return true;
    }
    return false;
}

} // namespace

void StepEntLogicAsMantrap(
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

    Ent& mantrap = state.ents.ents[ent_idx];
    if (mantrap.last_condition == EntCondition::Stunned &&
        mantrap.condition == EntCondition::Normal) {
        StartIdle(mantrap, state);
    }
    if (mantrap.condition != EntCondition::Normal) {
        return;
    }

    if (mantrap.counter_b > 0.0F) {
        mantrap.counter_b -= 1.0F;
        mantrap.vel.x = 0.0F;
        SetAnim(mantrap, aframe_ids::MantrapEat);
        return;
    }

    if (TryEatOverlappingEnt(ent_idx, state, graphics, audio)) {
        return;
    }

    if (mantrap.ai_state == EntAiState::Idle) {
        common::DecelerateHorizontallyToStop(mantrap, kMantrapWalkAcceleration);
        TrySetAnim(mantrap, EntDisplayState::Neutral);
        if (mantrap.counter_a > 0.0F) {
            mantrap.counter_a -= 1.0F;
            return;
        }

        mantrap.facing =
            state.drng.RandomIntInclusive(0, 1) == 0 ? Side::Left : Side::Right;
        StartWalking(mantrap, state);
        return;
    }

    int direction = mantrap.facing == Side::Left ? -1 : 1;
    if (common::HasWallAheadForGroundWalker(mantrap, state, graphics, direction) ||
        !common::HasGroundAheadForGroundWalker(mantrap, state, graphics, direction)) {
        mantrap.facing = mantrap.facing == Side::Left ? Side::Right : Side::Left;
        direction = -direction;
    }

    if (state.drng.RandomIntInclusive(1, kMantrapIdleChance) == 1) {
        StartIdle(mantrap, state);
        return;
    }

    common::AccelerateHorizontallyTowardSpeed(
        mantrap,
        state,
        static_cast<float>(direction) * kMantrapWalkSpeed,
        kMantrapWalkAcceleration
    );
    SetMovementFlag(mantrap, EntMovementFlag::Walking, true);
    TrySetAnim(mantrap, EntDisplayState::Walk);
}

extern const EntSpec kMantrapSpec{
    .type_ = EntType::Mantrap,
    .size = EntSpecSize(9.0F, 13.0F),
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
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .ai_state = EntAiState::Idle,
    .display_state = EntDisplayState::Neutral,
    .counter_a = static_cast<float>(kMantrapIdleMinFrames),
    .damage_vuln = DamageVuln::Vulnerable,
    .damage_anim = aframe_ids::BloodBall,
    .damage_sound = audio_asset_ids::CavemanHurt,
    .collide_sound = audio_asset_ids::Thud,
    .step_logic = StepEntLogicAsMantrap,
    .alignment = Alignment::Enemy,
    .aframe_animator = AFrameAnimator::New(aframe_ids::Mantrap),
};

} // namespace splonks::ents::mantrap

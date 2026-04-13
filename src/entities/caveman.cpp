#include "entities/caveman.hpp"
#include "on_damage_effects.hpp"

#include "audio.hpp"
#include "entities/common/common.hpp"
#include "entities/common/ground_walker.hpp"
#include "entity/archetype.hpp"
#include "entity/core_types.hpp"
#include "frame_data_animator.hpp"
#include "frame_data_id.hpp"
#include "math_types.hpp"
#include "state.hpp"

namespace splonks::entities::caveman {

namespace {

constexpr float kCavemanWalkSpeed = 0.8F;
constexpr int kCavemanIdleMinFrames = 24;
constexpr int kCavemanIdleMaxFrames = 64;
constexpr int kCavemanIdleChance = 120;

void StartIdle(Entity& caveman) {
    caveman.ai_state = EntityAiState::Idle;
    caveman.counter_a = static_cast<float>(rng::RandomIntInclusive(kCavemanIdleMinFrames, kCavemanIdleMaxFrames));
    caveman.vel.x = 0.0F;
    TrySetAnimation(caveman, EntityDisplayState::Neutral);
}

void StartWalking(Entity& caveman) {
    caveman.ai_state = EntityAiState::Patrolling;
    caveman.vel.x = caveman.facing == LeftOrRight::Left ? -kCavemanWalkSpeed : kCavemanWalkSpeed;
    TrySetAnimation(caveman, EntityDisplayState::Walk);
}

} // namespace

void StepEntityLogicAsCaveman(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    (void)dt;

    Entity& caveman = state.entity_manager.entities[entity_idx];
    if (caveman.condition != EntityCondition::Normal) {
        return;
    }

    if (caveman.ai_state == EntityAiState::Idle) {
        caveman.vel.x = 0.0F;
        TrySetAnimation(caveman, EntityDisplayState::Neutral);
        if (caveman.counter_a > 0.0F) {
            caveman.counter_a -= 1.0F;
            return;
        }

        caveman.facing = rng::RandomIntInclusive(0, 1) == 0 ? LeftOrRight::Left : LeftOrRight::Right;
        StartWalking(caveman);
        return;
    }

    int direction = caveman.facing == LeftOrRight::Left ? -1 : 1;
    if (common::HasWallAheadForGroundWalker(caveman, state, direction) ||
        !common::HasGroundAheadForGroundWalker(caveman, state, direction)) {
        caveman.facing = caveman.facing == LeftOrRight::Left ? LeftOrRight::Right : LeftOrRight::Left;
        direction = -direction;
    }

    if (rng::RandomIntInclusive(1, kCavemanIdleChance) == 1) {
        StartIdle(caveman);
        return;
    }

    caveman.vel.x = static_cast<float>(direction) * kCavemanWalkSpeed;
    SetMovementFlag(caveman, EntityMovementFlag::Walking, true);
    TrySetAnimation(caveman, EntityDisplayState::Walk);
}

extern const EntityArchetype kCavemanArchetype{
    .type_ = EntityType::Caveman,
    .size = Vec2::New(16.0F, 16.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = false,
    .impassable = false,
    .hurt_on_contact = true,
    .can_be_stunned = true,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .ai_state = EntityAiState::Idle,
    .display_state = EntityDisplayState::Neutral,
    .counter_a = static_cast<float>(kCavemanIdleMinFrames),
    .damage_vulnerability = DamageVulnerability::Vulnerable,
    .damage_animation = frame_data_ids::BloodBall,
    .collide_sound = SoundEffect::Thud,
    .step_logic = StepEntityLogicAsCaveman,
    .alignment = Alignment::Enemy,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::Caveman),
};

} // namespace splonks::entities::caveman

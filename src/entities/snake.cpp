#include "entities/snake.hpp"
#include "on_damage_effects.hpp"

#include "audio.hpp"
#include "entity/archetype.hpp"
#include "entities/common/common.hpp"
#include "entities/common/ground_walker.hpp"
#include "frame_data_id.hpp"
#include "state.hpp"
#include "tile.hpp"
#include "world_query.hpp"

namespace splonks::entities::snake {

namespace {

constexpr float kSnakeWalkSpeed = 1.0F;
constexpr int kSnakeIdleMinFrames = 20;
constexpr int kSnakeIdleMaxFrames = 50;
constexpr int kSnakeIdleChance = 100;

void StartIdle(Entity& snake) {
    snake.ai_state = EntityAiState::Idle;
    snake.counter_a = static_cast<float>(rng::RandomIntInclusive(kSnakeIdleMinFrames, kSnakeIdleMaxFrames));
    snake.vel.x = 0.0F;
    TrySetAnimation(snake, EntityDisplayState::Neutral);
}

void StartWalking(Entity& snake) {
    snake.ai_state = EntityAiState::Patrolling;
    snake.vel.x = snake.facing == LeftOrRight::Left ? -kSnakeWalkSpeed : kSnakeWalkSpeed;
    TrySetAnimation(snake, EntityDisplayState::Walk);
}

} // namespace

void StepEntityLogicAsSnake(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)audio;
    (void)dt;

    Entity& snake = state.entity_manager.entities[entity_idx];
    if (snake.condition != EntityCondition::Normal) {
        return;
    }

    if (snake.ai_state == EntityAiState::Idle) {
        snake.vel.x = 0.0F;
        TrySetAnimation(snake, EntityDisplayState::Neutral);
        if (snake.counter_a > 0.0F) {
            snake.counter_a -= 1.0F;
            return;
        }

        snake.facing = rng::RandomIntInclusive(0, 1) == 0 ? LeftOrRight::Left : LeftOrRight::Right;
        StartWalking(snake);
        return;
    }

    int direction = snake.facing == LeftOrRight::Left ? -1 : 1;
    if (common::HasWallAheadForGroundWalker(snake, state, graphics, direction) ||
        !common::HasGroundAheadForGroundWalker(snake, state, graphics, direction)) {
        snake.facing = snake.facing == LeftOrRight::Left ? LeftOrRight::Right : LeftOrRight::Left;
        direction = -direction;
    }

    if (rng::RandomIntInclusive(1, kSnakeIdleChance) == 1) {
        StartIdle(snake);
        return;
    }

    snake.vel.x = static_cast<float>(direction) * kSnakeWalkSpeed;
    SetMovementFlag(snake, EntityMovementFlag::Walking, true);
    TrySetAnimation(snake, EntityDisplayState::Walk);
}

extern const EntityArchetype kSnakeArchetype{
    .type_ = EntityType::Snake,
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
    .counter_a = static_cast<float>(kSnakeIdleMinFrames),
    .damage_vulnerability = DamageVulnerability::Vulnerable,
    .damage_animation = frame_data_ids::BloodBall,
    .collide_sound = SoundEffect::Thud,
    .step_logic = StepEntityLogicAsSnake,
    .alignment = Alignment::Enemy,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::Snake),
};

} // namespace splonks::entities::snake

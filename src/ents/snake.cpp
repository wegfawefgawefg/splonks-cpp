#include "ents/snake.hpp"
#include "on_damage_effects.hpp"

#include "audio.hpp"
#include "ent/spec.hpp"
#include "ents/common/common.hpp"
#include "ents/common/ground_walker.hpp"
#include "aframe_id.hpp"
#include "state.hpp"
#include "tile.hpp"
#include "world_query.hpp"

namespace splonks::ents::snake {

namespace {

constexpr float kSnakeWalkSpeed = 1.0F;
constexpr float kSnakeWalkAcceleration = 0.2F;
constexpr int kSnakeIdleMinFrames = 20;
constexpr int kSnakeIdleMaxFrames = 50;
constexpr int kSnakeIdleChance = 100;

void StartIdle(Ent& snake, State& state) {
    snake.ai_state = EntAiState::Idle;
    snake.counter_a = static_cast<float>(
        state.drng.RandomIntInclusive(kSnakeIdleMinFrames, kSnakeIdleMaxFrames));
    common::DecelerateHorizontallyToStop(snake, kSnakeWalkAcceleration);
    TrySetAnim(snake, EntDisplayState::Neutral);
}

void StartWalking(Ent& snake, const State& state) {
    snake.ai_state = EntAiState::Patrolling;
    common::AccelerateHorizontallyTowardSpeed(
        snake,
        state,
        snake.facing == Side::Left ? -kSnakeWalkSpeed : kSnakeWalkSpeed,
        kSnakeWalkAcceleration
    );
    TrySetAnim(snake, EntDisplayState::Walk);
}

} // namespace

void StepEntLogicAsSnake(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)audio;
    (void)dt;

    Ent& snake = state.ents.ents[ent_idx];
    if (snake.condition != EntCondition::Normal) {
        return;
    }

    if (snake.ai_state == EntAiState::Idle) {
        common::DecelerateHorizontallyToStop(snake, kSnakeWalkAcceleration);
        TrySetAnim(snake, EntDisplayState::Neutral);
        if (snake.counter_a > 0.0F) {
            snake.counter_a -= 1.0F;
            return;
        }

        snake.facing =
            state.drng.RandomIntInclusive(0, 1) == 0 ? Side::Left : Side::Right;
        StartWalking(snake, state);
        return;
    }

    int direction = snake.facing == Side::Left ? -1 : 1;
    if (common::HasWallAheadForGroundWalker(snake, state, graphics, direction) ||
        !common::HasGroundAheadForGroundWalker(snake, state, graphics, direction)) {
        snake.facing = snake.facing == Side::Left ? Side::Right : Side::Left;
        direction = -direction;
    }

    if (state.drng.RandomIntInclusive(1, kSnakeIdleChance) == 1) {
        StartIdle(snake, state);
        return;
    }

    common::AccelerateHorizontallyTowardSpeed(
        snake,
        state,
        static_cast<float>(direction) * kSnakeWalkSpeed,
        kSnakeWalkAcceleration
    );
    SetMovementFlag(snake, EntMovementFlag::Walking, true);
    TrySetAnim(snake, EntDisplayState::Walk);
}

extern const EntSpec kSnakeSpec{
    .type_ = EntType::Snake,
    .size = EntSpecSize(16.0F, 16.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .can_only_be_picked_up_if_dead_or_stunned = true,
    .impassable = false,
    .hurt_on_contact = true,
    .can_be_stunned = true,
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .ai_state = EntAiState::Idle,
    .display_state = EntDisplayState::Neutral,
    .counter_a = EntSpecCounter(static_cast<float>(kSnakeIdleMinFrames)),
    .damage_vuln = DamageVuln::Vulnerable,
    .damage_anim = aframe_ids::BloodBall,
    .collide_sound = audio_asset_ids::Thud,
    .step_logic = StepEntLogicAsSnake,
    .alignment = Alignment::Enemy,
    .aframe_animator = AFrameAnimator::New(aframe_ids::Snake),
};

} // namespace splonks::ents::snake

#include "entities/moving_platform.hpp"

#include "entities/common/common.hpp"
#include "frame_data_id.hpp"

#include <cmath>

namespace splonks::entities::moving_platform {

namespace {

constexpr float kPlatformSpeed = 1.0F;
constexpr float kCircleAngularSpeed = 0.08F;

void StepHorizontalPingPong(Entity& platform) {
    const float min_x = static_cast<float>(platform.point_a.x);
    const float max_x = static_cast<float>(platform.point_b.x);
    if (platform.counter_b == 0.0F) {
        platform.counter_b = 1.0F;
    }

    if (platform.counter_b > 0.0F && platform.pos.x >= max_x) {
        platform.pos.x = max_x;
        platform.counter_b = -1.0F;
    } else if (platform.counter_b < 0.0F && platform.pos.x <= min_x) {
        platform.pos.x = min_x;
        platform.counter_b = 1.0F;
    }

    platform.vel = Vec2::New(platform.counter_b * kPlatformSpeed, 0.0F);
}

void StepVerticalPingPong(Entity& platform) {
    const float min_y = static_cast<float>(platform.point_a.y);
    const float max_y = static_cast<float>(platform.point_b.y);
    if (platform.counter_b == 0.0F) {
        platform.counter_b = 1.0F;
    }

    if (platform.counter_b > 0.0F && platform.pos.y >= max_y) {
        platform.pos.y = max_y;
        platform.counter_b = -1.0F;
    } else if (platform.counter_b < 0.0F && platform.pos.y <= min_y) {
        platform.pos.y = min_y;
        platform.counter_b = 1.0F;
    }

    platform.vel = Vec2::New(0.0F, platform.counter_b * kPlatformSpeed);
}

void StepCircle(Entity& platform) {
    const Vec2 center = ToVec2(platform.point_a);
    const float radius = platform.threshold_a;
    const Vec2 desired_pos = center +
                             Vec2::New(
                                 std::round(std::cos(platform.counter_a) * radius),
                                 std::round(std::sin(platform.counter_a) * radius)
                             );
    platform.vel = desired_pos - platform.pos;
    platform.counter_a += kCircleAngularSpeed;
}

} // namespace

extern const EntityArchetype kMovingPlatformArchetype{
    .type_ = EntityType::MovingPlatform,
    .size = Vec2::New(28.0F, 28.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_hit = false,
    .can_be_picked_up = false,
    .impassable = true,
    .hurt_on_contact = false,
    .crusher_pusher = true,
    .can_stomp = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .has_ground_friction = false,
    .draw_layer = DrawLayer::Middle,
    .facing = LeftOrRight::Right,
    .condition = EntityCondition::Normal,
    .ai_state = EntityAiState::Idle,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Immune,
    .step_logic = StepEntityLogicAsMovingPlatform,
    .step_physics = StepEntityPhysicsAsMovingPlatform,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(HashFrameDataIdConstexpr("boulder")),
};

void StepEntityLogicAsMovingPlatform(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    (void)dt;

    Entity& platform = state.entity_manager.entities[entity_idx];
    switch (platform.ai_state) {
    case EntityAiState::Idle:
        StepHorizontalPingPong(platform);
        break;
    case EntityAiState::Patrolling:
        StepVerticalPingPong(platform);
        break;
    case EntityAiState::Disturbed:
        StepCircle(platform);
        break;
    case EntityAiState::Pursuing:
    case EntityAiState::Returning:
        platform.vel = Vec2::New(0.0F, 0.0F);
        break;
    }
}

void StepEntityPhysicsAsMovingPlatform(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    common::PrePartialEulerStep(entity_idx, state, dt);
    common::DoTileAndEntityCollisions(entity_idx, state, graphics, audio);
    common::PostPartialEulerStep(entity_idx, state, dt);
}

} // namespace splonks::entities::moving_platform

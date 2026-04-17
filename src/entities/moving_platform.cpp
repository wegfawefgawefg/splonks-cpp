#include "entities/moving_platform.hpp"

#include "entities/common/common.hpp"
#include "particles/ultra_dynamic_particle.hpp"
#include "frame_data_id.hpp"
#include "state.hpp"

#include <cmath>
#include <memory>

namespace splonks::entities::moving_platform {

namespace {

constexpr float kPlatformSpeed = 1.0F;
constexpr float kCircleAngularSpeed = 0.08F;
constexpr float kIcyPlatformFriction = 1.0F;

bool IsIcyPlatform(const Entity& platform) {
    return platform.impassable &&
           !platform.can_be_hung_on &&
           platform.support_ground_friction >= kIcyPlatformFriction;
}

void SpawnIcyPlatformParticles(const Entity& platform, State& state) {
    if ((state.stage_frame + platform.vid.id) % 8U != 0U) {
        return;
    }

    auto shard = std::make_unique<UltraDynamicParticle>();
    shard->frame_data_animator = FrameDataAnimator::New(frame_data_ids::IceBlock);
    shard->draw_layer = DrawLayer::Foreground;
    shard->counter = static_cast<std::uint32_t>(rng::RandomIntExclusive(12, 20));
    shard->pos = platform.GetCenter() + Vec2::New(
        rng::RandomFloat(-4.0F, 4.0F),
        rng::RandomFloat(-2.0F, 2.0F)
    );
    const float size = rng::RandomFloat(3.0F, 5.0F);
    shard->size = Vec2::New(size, size);
    shard->rot = rng::RandomFloat(0.0F, 360.0F);
    shard->alpha = rng::RandomFloat(0.55F, 0.85F);
    shard->vel = Vec2::New(rng::RandomFloat(-0.35F, 0.35F), rng::RandomFloat(-0.4F, -0.1F));
    shard->svel = Vec2::New(0.0F, 0.0F);
    shard->rotvel = rng::RandomFloat(-0.25F, 0.25F);
    shard->alpha_vel = -0.03F;
    shard->acc = Vec2::New(0.0F, 0.02F);
    shard->sacc = Vec2::New(0.0F, 0.0F);
    shard->rotacc = 0.0F;
    shard->alpha_acc = 0.0F;
    state.particles.Add(std::move(shard));
}

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
    .affected_by_ground_friction = false,
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

    if (IsIcyPlatform(platform)) {
        SpawnIcyPlatformParticles(platform, state);
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

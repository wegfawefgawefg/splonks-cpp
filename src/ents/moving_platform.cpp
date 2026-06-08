#include "ents/moving_platform.hpp"

#include "ents/common/common.hpp"
#include "aframe_id.hpp"
#include "particles/sprite_particle.hpp"
#include "sim/fxp.hpp"
#include "state.hpp"

#include <array>
#include <cstdint>
#include <memory>

namespace splonks::ents::moving_platform {

namespace {

constexpr float kPlatformSpeed = 1.0F;
constexpr float kIcyPlatformFriction = 1.0F;
constexpr int kCircleUnitScale = 4096;

struct CircleUnit {
    int x = 0;
    int y = 0;

    static constexpr CircleUnit New(int x_value, int y_value) {
        return CircleUnit{x_value, y_value};
    }
};

constexpr std::array<CircleUnit, 80> kCirclePath{{
    CircleUnit::New(4096, 0), CircleUnit::New(4083, 321), CircleUnit::New(4046, 641), CircleUnit::New(3983, 956),
    CircleUnit::New(3896, 1266), CircleUnit::New(3784, 1567), CircleUnit::New(3650, 1860), CircleUnit::New(3492, 2140),
    CircleUnit::New(3314, 2408), CircleUnit::New(3115, 2660), CircleUnit::New(2896, 2896), CircleUnit::New(2660, 3115),
    CircleUnit::New(2408, 3314), CircleUnit::New(2140, 3492), CircleUnit::New(1860, 3650), CircleUnit::New(1567, 3784),
    CircleUnit::New(1266, 3896), CircleUnit::New(956, 3983), CircleUnit::New(641, 4046), CircleUnit::New(321, 4083),
    CircleUnit::New(0, 4096), CircleUnit::New(-321, 4083), CircleUnit::New(-641, 4046), CircleUnit::New(-956, 3983),
    CircleUnit::New(-1266, 3896), CircleUnit::New(-1567, 3784), CircleUnit::New(-1860, 3650), CircleUnit::New(-2140, 3492),
    CircleUnit::New(-2408, 3314), CircleUnit::New(-2660, 3115), CircleUnit::New(-2896, 2896), CircleUnit::New(-3115, 2660),
    CircleUnit::New(-3314, 2408), CircleUnit::New(-3492, 2140), CircleUnit::New(-3650, 1860), CircleUnit::New(-3784, 1567),
    CircleUnit::New(-3896, 1266), CircleUnit::New(-3983, 956), CircleUnit::New(-4046, 641), CircleUnit::New(-4083, 321),
    CircleUnit::New(-4096, 0), CircleUnit::New(-4083, -321), CircleUnit::New(-4046, -641), CircleUnit::New(-3983, -956),
    CircleUnit::New(-3896, -1266), CircleUnit::New(-3784, -1567), CircleUnit::New(-3650, -1860), CircleUnit::New(-3492, -2140),
    CircleUnit::New(-3314, -2408), CircleUnit::New(-3115, -2660), CircleUnit::New(-2896, -2896), CircleUnit::New(-2660, -3115),
    CircleUnit::New(-2408, -3314), CircleUnit::New(-2140, -3492), CircleUnit::New(-1860, -3650), CircleUnit::New(-1567, -3784),
    CircleUnit::New(-1266, -3896), CircleUnit::New(-956, -3983), CircleUnit::New(-641, -4046), CircleUnit::New(-321, -4083),
    CircleUnit::New(0, -4096), CircleUnit::New(321, -4083), CircleUnit::New(641, -4046), CircleUnit::New(956, -3983),
    CircleUnit::New(1266, -3896), CircleUnit::New(1567, -3784), CircleUnit::New(1860, -3650), CircleUnit::New(2140, -3492),
    CircleUnit::New(2408, -3314), CircleUnit::New(2660, -3115), CircleUnit::New(2896, -2896), CircleUnit::New(3115, -2660),
    CircleUnit::New(3314, -2408), CircleUnit::New(3492, -2140), CircleUnit::New(3650, -1860), CircleUnit::New(3784, -1567),
    CircleUnit::New(3896, -1266), CircleUnit::New(3983, -956), CircleUnit::New(4046, -641), CircleUnit::New(4083, -321)
}};

int RoundFloatToInt(float value) {
    return static_cast<int>(value + (value >= 0.0F ? 0.5F : -0.5F));
}

int RoundRatio(std::int64_t numerator, std::int64_t denominator) {
    if (denominator == 0) {
        return 0;
    }
    const std::int64_t half = denominator / 2;
    if (numerator >= 0) {
        return static_cast<int>((numerator + half) / denominator);
    }
    return static_cast<int>((numerator - half) / denominator);
}

int PositiveModulo(int value, int divisor) {
    const int result = value % divisor;
    return result < 0 ? result + divisor : result;
}

bool IsIcyPlatform(const Ent& platform) {
    return platform.impassable &&
           !platform.can_be_hung_on &&
           platform.support_ground_friction >= sim::ToSimScalar(kIcyPlatformFriction);
}

void SpawnIcyPlatformParticles(const Ent& platform, State& state) {
    if ((state.stage_frame + platform.vid.id) % 8U != 0U) {
        return;
    }

    SpriteParticle shard{};
    shard.aframe_animator = AFrameAnimator::New(aframe_ids::IceBlock);
    shard.draw_layer = DrawLayer::Foreground;
    shard.counter = static_cast<std::uint32_t>(rng::RandomIntExclusive(12, 20));
    shard.pos = platform.GetRenderCenter() + Vec2::New(
        rng::RandomFloat(-4.0F, 4.0F),
        rng::RandomFloat(-2.0F, 2.0F)
    );
    const float size = rng::RandomFloat(3.0F, 5.0F);
    shard.size = Vec2::New(size, size);
    shard.rot = rng::RandomFloat(0.0F, 360.0F);
    shard.alpha = rng::RandomFloat(0.55F, 0.85F);
    shard.vel = Vec2::New(rng::RandomFloat(-0.35F, 0.35F), rng::RandomFloat(-0.4F, -0.1F));
    shard.svel = Vec2::New(0.0F, 0.0F);
    shard.rotvel = rng::RandomFloat(-0.25F, 0.25F);
    shard.alpha_vel = -0.03F;
    shard.acc = Vec2::New(0.0F, 0.02F);
    shard.sacc = Vec2::New(0.0F, 0.0F);
    shard.rotacc = 0.0F;
    shard.alpha_acc = 0.0F;
    state.particles.Add(std::move(shard));
}

void StepHorizontalPingPong(Ent& platform) {
    const sim::Scalar min_x = sim::Scalar::from_int(platform.point_a.x);
    const sim::Scalar max_x = sim::Scalar::from_int(platform.point_b.x);
    if (platform.counter_b == sim::Scalar::zero()) {
        platform.counter_b = sim::Scalar::from_int(1);
    }

    if (platform.counter_b > sim::Scalar::zero() && platform.pos.x >= max_x) {
        platform.pos.x = max_x;
        platform.counter_b = -sim::Scalar::from_int(1);
    } else if (platform.counter_b < sim::Scalar::zero() && platform.pos.x <= min_x) {
        platform.pos.x = min_x;
        platform.counter_b = sim::Scalar::from_int(1);
    }

    platform.vel = sim::Vec2{platform.counter_b * sim::ToSimScalar(kPlatformSpeed),
                             sim::Scalar::zero()};
}

void StepVerticalPingPong(Ent& platform) {
    const sim::Scalar min_y = sim::Scalar::from_int(platform.point_a.y);
    const sim::Scalar max_y = sim::Scalar::from_int(platform.point_b.y);
    if (platform.counter_b == sim::Scalar::zero()) {
        platform.counter_b = sim::Scalar::from_int(1);
    }

    if (platform.counter_b > sim::Scalar::zero() && platform.pos.y >= max_y) {
        platform.pos.y = max_y;
        platform.counter_b = -sim::Scalar::from_int(1);
    } else if (platform.counter_b < sim::Scalar::zero() && platform.pos.y <= min_y) {
        platform.pos.y = min_y;
        platform.counter_b = sim::Scalar::from_int(1);
    }

    platform.vel = sim::Vec2{sim::Scalar::zero(),
                             platform.counter_b * sim::ToSimScalar(kPlatformSpeed)};
}

void StepCircle(Ent& platform) {
    const sim::Vec2 center = sim::PixelVec2(platform.point_a.x, platform.point_a.y);
    const int radius = platform.threshold_a.to_pixels_round();
    const int path_idx = PositiveModulo(
        platform.counter_a.trunc_int(),
        static_cast<int>(kCirclePath.size())
    );
    const CircleUnit unit = kCirclePath[static_cast<std::size_t>(path_idx)];
    const sim::Vec2 desired_pos = center + sim::PixelVec2(
        RoundRatio(static_cast<std::int64_t>(unit.x) * radius, kCircleUnitScale),
        RoundRatio(static_cast<std::int64_t>(unit.y) * radius, kCircleUnitScale)
    );
    platform.vel = desired_pos - platform.pos;
    platform.counter_a += sim::Scalar::from_int(1);
    const sim::Scalar circle_path_size =
        sim::Scalar::from_int(static_cast<int>(kCirclePath.size()));
    if (platform.counter_a >= circle_path_size) {
        platform.counter_a -= circle_path_size;
    }
}

} // namespace

extern const EntSpec kMovingPlatformSpec{
    .type_ = EntType::MovingPlatform,
    .size = EntSpecSize(28.0F, 28.0F),
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
    .facing = Side::Right,
    .condition = EntCondition::Normal,
    .ai_state = EntAiState::Idle,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::Immune,
    .step_logic = StepEntLogicAsMovingPlatform,
    .step_physics = StepEntPhysicsAsMovingPlatform,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(HashAFrameIdConstexpr("boulder")),
};

void StepEntLogicAsMovingPlatform(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    (void)dt;

    Ent& platform = state.ents.ents[ent_idx];
    switch (platform.ai_state) {
    case EntAiState::Idle:
        StepHorizontalPingPong(platform);
        break;
    case EntAiState::Patrolling:
        StepVerticalPingPong(platform);
        break;
    case EntAiState::Disturbed:
        StepCircle(platform);
        break;
    case EntAiState::Pursuing:
    case EntAiState::Returning:
        platform.vel = sim::Vec2::zero();
        break;
    }

    if (IsIcyPlatform(platform)) {
        SpawnIcyPlatformParticles(platform, state);
    }
}

void StepEntPhysicsAsMovingPlatform(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    common::PrePartialEulerStep(ent_idx, state, dt);
    common::DoTileAndEntCollisions(ent_idx, state, graphics, audio);
    common::PostPartialEulerStep(ent_idx, state, dt);
}

} // namespace splonks::ents::moving_platform

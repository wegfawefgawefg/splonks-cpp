#include "ents/ball_and_chain.hpp"

#include "ent/spec.hpp"
#include "aframe_id.hpp"
#include "player_queries.hpp"
#include "state.hpp"
#include "world_query.hpp"

#include <algorithm>
#include <cstdint>

namespace splonks::ents::ball_and_chain {

namespace {

constexpr std::int32_t kFixedScale = 4096;
constexpr std::int32_t kDistanceEpsilonRaw = 4;
constexpr std::int32_t kChainLengthRaw = 26 * kFixedScale;
// Raw fixed-point forms of 0.32, 0.52, and 1.35.
constexpr std::int32_t kBallCatchupAccelerationRaw = 1311;
constexpr std::int32_t kChainPullAccelerationRaw = 2130;
constexpr std::int32_t kPlayerAwayVelocityDampingPercent = 85;
constexpr std::int32_t kMaxPlayerPullRaw = 5529;

Vec2 GetAnchorPos(const Ent& player) {
    return player.GetCenter() + Vec2::New(0.0F, (player.GetSize().y * 0.5F) - 1.0F);
}

std::int32_t RoundFloatToFixedRaw(float value) {
    const float scaled = value * static_cast<float>(kFixedScale);
    return static_cast<std::int32_t>(scaled + (scaled >= 0.0F ? 0.5F : -0.5F));
}

std::int32_t RoundRatio(std::int64_t numerator, std::int64_t denominator) {
    if (denominator == 0) {
        return 0;
    }
    if (denominator < 0) {
        numerator = -numerator;
        denominator = -denominator;
    }

    const std::int64_t half = denominator / 2;
    if (numerator >= 0) {
        return static_cast<std::int32_t>((numerator + half) / denominator);
    }
    return static_cast<std::int32_t>((numerator - half) / denominator);
}

std::uint64_t IntSqrtFloor(std::uint64_t value) {
    std::uint64_t result = 0;
    std::uint64_t bit = std::uint64_t{1} << 62;
    while (bit > value) {
        bit >>= 2;
    }

    while (bit != 0) {
        if (value >= result + bit) {
            value -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }

    return result;
}

std::int32_t IntSqrtRound(std::uint64_t value) {
    const std::uint64_t floor = IntSqrtFloor(value);
    const std::uint64_t low_delta = value - (floor * floor);
    const std::uint64_t next = floor + 1;
    const std::uint64_t high_delta = (next * next) - value;
    return static_cast<std::int32_t>(high_delta < low_delta ? next : floor);
}

float FixedRawToFloat(std::int32_t raw) {
    return static_cast<float>(raw) / static_cast<float>(kFixedScale);
}

Vec2 FixedDeltaToVec2(std::int32_t x_raw, std::int32_t y_raw) {
    return Vec2::New(FixedRawToFloat(x_raw), FixedRawToFloat(y_raw));
}

} // namespace

extern const EntSpec kBallAndChainBallSpec{
    .type_ = EntType::BallAndChainBall,
    .size = EntSpecSize(8.0F, 8.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::Immune,
    .step_logic = StepEntLogicAsBallAndChainBall,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::RopeBall),
};

void StepEntLogicAsBallAndChainBall(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    (void)dt;

    Ent& ball = state.ents.ents[ent_idx];
    SetAnim(ball, aframe_ids::RopeBall);

    if (!ball.ent_a.has_value()) {
        ball.active = false;
        return;
    }

    Ent* const player = state.ents.GetEntMut(*ball.ent_a);
    if (player == nullptr || !player->active || player->condition == EntCondition::Dead) {
        if (Ent* const current_player = GetPrimaryLocalPlayerMut(state);
            current_player != nullptr && current_player->ent_d.has_value() &&
            *current_player->ent_d == ball.vid) {
            current_player->ent_d.reset();
        }
        ball.active = false;
        return;
    }

    const bool held_by_player = player->holding_vid.has_value() && *player->holding_vid == ball.vid;
    const bool launched = ball.thrown_by.has_value() && ball.proj_contact_timer > 0;
    if (!launched) {
        ball.thrown_by.reset();
        ball.thrown_immunity_timer = 0;
        ball.proj_contact_timer = 0;
    }
    if (held_by_player) {
        return;
    }

    const Vec2 anchor_pos = GetAnchorPos(*player);
    const Vec2 ball_center = GetNearestWorldPoint(state.stage, anchor_pos, ball.GetCenter());
    const Vec2 delta = ball_center - anchor_pos;
    const std::int32_t delta_x_raw = RoundFloatToFixedRaw(delta.x);
    const std::int32_t delta_y_raw = RoundFloatToFixedRaw(delta.y);
    const std::uint64_t distance_sq =
        static_cast<std::uint64_t>(static_cast<std::int64_t>(delta_x_raw) * delta_x_raw) +
        static_cast<std::uint64_t>(static_cast<std::int64_t>(delta_y_raw) * delta_y_raw);
    const std::int32_t distance_raw = IntSqrtRound(distance_sq);
    if (distance_raw <= kDistanceEpsilonRaw) {
        return;
    }

    const std::int32_t dir_x_raw =
        RoundRatio(static_cast<std::int64_t>(delta_x_raw) * kFixedScale, distance_raw);
    const std::int32_t dir_y_raw =
        RoundRatio(static_cast<std::int64_t>(delta_y_raw) * kFixedScale, distance_raw);
    const std::int32_t catchup_x_raw =
        RoundRatio(static_cast<std::int64_t>(dir_x_raw) * kBallCatchupAccelerationRaw, kFixedScale);
    const std::int32_t catchup_y_raw =
        RoundRatio(static_cast<std::int64_t>(dir_y_raw) * kBallCatchupAccelerationRaw, kFixedScale);
    ball.acc = ball.acc - FixedDeltaToVec2(catchup_x_raw, catchup_y_raw);
    if (distance_raw <= kChainLengthRaw) {
        return;
    }

    const std::int32_t tautness_raw = distance_raw - kChainLengthRaw;
    const std::int32_t pull_acceleration_raw =
        kChainPullAccelerationRaw + RoundRatio(tautness_raw, 100);
    const std::int32_t pull_x_raw =
        RoundRatio(static_cast<std::int64_t>(dir_x_raw) * pull_acceleration_raw, kFixedScale);
    const std::int32_t pull_y_raw =
        RoundRatio(static_cast<std::int64_t>(dir_y_raw) * pull_acceleration_raw, kFixedScale);
    ball.acc = ball.acc - FixedDeltaToVec2(pull_x_raw, pull_y_raw);

    const std::int32_t player_vel_x_raw = RoundFloatToFixedRaw(player->vel.x);
    const std::int32_t player_vel_y_raw = RoundFloatToFixedRaw(player->vel.y);
    const std::int32_t player_away_speed_raw = RoundRatio(
        (static_cast<std::int64_t>(player_vel_x_raw) * dir_x_raw) +
        (static_cast<std::int64_t>(player_vel_y_raw) * dir_y_raw),
        kFixedScale
    );
    if (player_away_speed_raw > 0) {
        const std::int32_t damped_speed_raw = std::min(
            RoundRatio(
                static_cast<std::int64_t>(player_away_speed_raw) *
                    kPlayerAwayVelocityDampingPercent,
                100
            ),
            kMaxPlayerPullRaw
        );
        const std::int32_t pullback_x_raw =
            RoundRatio(static_cast<std::int64_t>(dir_x_raw) * damped_speed_raw, kFixedScale);
        const std::int32_t pullback_y_raw =
            RoundRatio(static_cast<std::int64_t>(dir_y_raw) * damped_speed_raw, kFixedScale);
        player->vel = player->vel - FixedDeltaToVec2(pullback_x_raw, pullback_y_raw);
    }
}

} // namespace splonks::ents::ball_and_chain

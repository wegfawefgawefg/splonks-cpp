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

using FixedRaw = sim::Scalar::raw_type;

constexpr FixedRaw kFixedScale = sim::Scalar::scale;
constexpr sim::Scalar kDistanceEpsilon = sim::Scalar::from_raw(4);
constexpr sim::Scalar kChainLength = sim::Scalar::from_int(26);
constexpr sim::Scalar kBallCatchupAcceleration = sim::Scalar::from_raw(1311);
constexpr sim::Scalar kChainPullAcceleration = sim::Scalar::from_raw(2130);
constexpr std::int32_t kPlayerAwayVelocityDampingPercent = 85;
constexpr sim::Scalar kMaxPlayerPull = sim::Scalar::from_raw(5529);

sim::FxVec2 GetAnchorPos(const Ent& player) {
    return player.GetCenter() +
           sim::FxVec2{sim::Scalar::zero(), (player.size.y / 2) - sim::Scalar::from_int(1)};
}

FixedRaw RoundRatio(std::int64_t numerator, std::int64_t denominator) {
    if (denominator == 0) {
        return 0;
    }
    if (denominator < 0) {
        numerator = -numerator;
        denominator = -denominator;
    }

    const std::int64_t half = denominator / 2;
    if (numerator >= 0) {
        return static_cast<FixedRaw>((numerator + half) / denominator);
    }
    return static_cast<FixedRaw>((numerator - half) / denominator);
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

FixedRaw IntSqrtRound(std::uint64_t value) {
    const std::uint64_t floor = IntSqrtFloor(value);
    const std::uint64_t low_delta = value - (floor * floor);
    const std::uint64_t next = floor + 1;
    const std::uint64_t high_delta = (next * next) - value;
    return static_cast<FixedRaw>(high_delta < low_delta ? next : floor);
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

    const sim::FxVec2 anchor_pos = GetAnchorPos(*player);
    const sim::FxVec2 ball_center = GetNearestWorldPoint(state.stage, anchor_pos, ball.GetCenter());
    const sim::FxVec2 delta = ball_center - anchor_pos;
    const FixedRaw delta_x_raw = delta.x.raw_value();
    const FixedRaw delta_y_raw = delta.y.raw_value();
    const std::uint64_t distance_sq =
        static_cast<std::uint64_t>(static_cast<std::int64_t>(delta_x_raw) * delta_x_raw) +
        static_cast<std::uint64_t>(static_cast<std::int64_t>(delta_y_raw) * delta_y_raw);
    const FixedRaw distance_raw = IntSqrtRound(distance_sq);
    if (distance_raw <= kDistanceEpsilon.raw_value()) {
        return;
    }

    const FixedRaw dir_x_raw =
        RoundRatio(static_cast<std::int64_t>(delta_x_raw) * kFixedScale, distance_raw);
    const FixedRaw dir_y_raw =
        RoundRatio(static_cast<std::int64_t>(delta_y_raw) * kFixedScale, distance_raw);
    const FixedRaw catchup_x_raw = RoundRatio(
        static_cast<std::int64_t>(dir_x_raw) * kBallCatchupAcceleration.raw_value(),
        kFixedScale
    );
    const FixedRaw catchup_y_raw = RoundRatio(
        static_cast<std::int64_t>(dir_y_raw) * kBallCatchupAcceleration.raw_value(),
        kFixedScale
    );
    ball.acc = ball.acc - sim::FxVec2::from_raw(catchup_x_raw, catchup_y_raw);
    if (distance_raw <= kChainLength.raw_value()) {
        return;
    }

    const FixedRaw tautness_raw = distance_raw - kChainLength.raw_value();
    const FixedRaw pull_acceleration_raw =
        kChainPullAcceleration.raw_value() + RoundRatio(tautness_raw, 100);
    const FixedRaw pull_x_raw =
        RoundRatio(static_cast<std::int64_t>(dir_x_raw) * pull_acceleration_raw, kFixedScale);
    const FixedRaw pull_y_raw =
        RoundRatio(static_cast<std::int64_t>(dir_y_raw) * pull_acceleration_raw, kFixedScale);
    ball.acc = ball.acc - sim::FxVec2::from_raw(pull_x_raw, pull_y_raw);

    const FixedRaw player_vel_x_raw = player->vel.x.raw_value();
    const FixedRaw player_vel_y_raw = player->vel.y.raw_value();
    const FixedRaw player_away_speed_raw = RoundRatio(
        (static_cast<std::int64_t>(player_vel_x_raw) * dir_x_raw) +
        (static_cast<std::int64_t>(player_vel_y_raw) * dir_y_raw),
        kFixedScale
    );
    if (player_away_speed_raw > 0) {
        const FixedRaw damped_speed_raw = std::min(
            RoundRatio(
                static_cast<std::int64_t>(player_away_speed_raw) *
                    kPlayerAwayVelocityDampingPercent,
                100
            ),
            kMaxPlayerPull.raw_value()
        );
        const FixedRaw pullback_x_raw =
            RoundRatio(static_cast<std::int64_t>(dir_x_raw) * damped_speed_raw, kFixedScale);
        const FixedRaw pullback_y_raw =
            RoundRatio(static_cast<std::int64_t>(dir_y_raw) * damped_speed_raw, kFixedScale);
        player->vel = player->vel - sim::FxVec2::from_raw(pullback_x_raw, pullback_y_raw);
    }
}

} // namespace splonks::ents::ball_and_chain

#include "ents/ball_and_chain.hpp"

#include "ent/spec.hpp"
#include "aframe_id.hpp"
#include "player_queries.hpp"
#include "state.hpp"
#include "world_query.hpp"

#include <algorithm>
#include <cmath>

namespace splonks::ents::ball_and_chain {

namespace {

constexpr float kChainLength = 26.0F;
constexpr float kBallCatchupAcceleration = 0.32F;
constexpr float kChainPullAcceleration = 0.52F;
constexpr float kPlayerAwayVelocityDamping = 0.85F;

Vec2 GetAnchorPos(const Ent& player) {
    return player.GetCenter() + Vec2::New(0.0F, (player.size.y * 0.5F) - 1.0F);
}

} // namespace

extern const EntSpec kBallAndChainBallSpec{
    .type_ = EntType::BallAndChainBall,
    .size = Vec2::New(8.0F, 8.0F),
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
    const float distance = std::sqrt((delta.x * delta.x) + (delta.y * delta.y));
    if (distance <= 0.001F) {
        return;
    }

    const Vec2 dir = delta / distance;
    ball.acc = ball.acc - (dir * kBallCatchupAcceleration);
    if (distance <= kChainLength) {
        return;
    }

    const float tautness = distance - kChainLength;
    ball.acc = ball.acc - (dir * (kChainPullAcceleration + (tautness * 0.01F)));

    const float player_away_speed = (player->vel.x * dir.x) + (player->vel.y * dir.y);
    if (player_away_speed > 0.0F) {
        player->vel = player->vel - (dir * std::min(player_away_speed * kPlayerAwayVelocityDamping, 1.35F));
    }
}

} // namespace splonks::ents::ball_and_chain

#include "entities/ball_and_chain.hpp"

#include "entity/archetype.hpp"
#include "frame_data_id.hpp"
#include "player_queries.hpp"
#include "state.hpp"
#include "world_query.hpp"

#include <algorithm>
#include <cmath>

namespace splonks::entities::ball_and_chain {

namespace {

constexpr float kChainLength = 26.0F;
constexpr float kBallCatchupAcceleration = 0.32F;
constexpr float kChainPullAcceleration = 0.52F;
constexpr float kPlayerAwayVelocityDamping = 0.85F;

Vec2 GetAnchorPos(const Entity& player) {
    return player.GetCenter() + Vec2::New(0.0F, (player.size.y * 0.5F) - 1.0F);
}

} // namespace

extern const EntityArchetype kBallAndChainBallArchetype{
    .type_ = EntityType::BallAndChainBall,
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
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Immune,
    .step_logic = StepEntityLogicAsBallAndChainBall,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::RopeBall),
};

void StepEntityLogicAsBallAndChainBall(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    (void)dt;

    Entity& ball = state.entity_manager.entities[entity_idx];
    SetAnimation(ball, frame_data_ids::RopeBall);

    if (!ball.entity_a.has_value()) {
        ball.active = false;
        return;
    }

    Entity* const player = state.entity_manager.GetEntityMut(*ball.entity_a);
    if (player == nullptr || !player->active || player->condition == EntityCondition::Dead) {
        if (Entity* const current_player = GetPrimaryLocalPlayerMut(state);
            current_player != nullptr && current_player->entity_d.has_value() &&
            *current_player->entity_d == ball.vid) {
            current_player->entity_d.reset();
        }
        ball.active = false;
        return;
    }

    const bool held_by_player = player->holding_vid.has_value() && *player->holding_vid == ball.vid;
    const bool launched = ball.thrown_by.has_value() && ball.projectile_contact_timer > 0;
    if (!launched) {
        ball.thrown_by.reset();
        ball.thrown_immunity_timer = 0;
        ball.projectile_contact_timer = 0;
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

} // namespace splonks::entities::ball_and_chain

#include "entities/ghost_ball.hpp"

#include "entity_archetype.hpp"
#include "entities/common.hpp"
#include "frame_data_id.hpp"
#include "state.hpp"

namespace splonks::entities::ghost_ball {

namespace {

Vec2 NormalizeOrZero(const Vec2& value) {
    const float length = Length(value);
    if (length == 0.0F) {
        return Vec2::New(0.0F, 0.0F);
    }
    return value / length;
}

} // namespace

extern const EntityArchetype kGhostBallArchetype{
    .type_ = EntityType::GhostBall,
    .size = Vec2::New(1.0F, 1.0F),
    .has_physics = true,
    .can_collide = false,
    .super_state = EntitySuperState::Idle,
    .state = EntityState::Idle,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Vulnerable,
    .entity_label_a = EntityLabel::GoToThis,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::NoSprite),
};

void StepEntityLogicAsGhostBall(std::size_t entity_idx, State& state) {
    // the ghostball should always chase the player.

    {
        Entity& ghost_ball = state.entity_manager.entities[entity_idx];
        // check if a there is a target.
        // if no, target the player.
        if (!ghost_ball.entity_a.has_value()) {
            if (state.player_vid.has_value()) {
                ghost_ball.entity_a = state.player_vid;
            }
        }
    }

    // try to fetch the target
    // set your acceleration towards your target vid
    Entity& ghost_ball = state.entity_manager.entities[entity_idx];
    Vec2 target_position = ghost_ball.pos;
    if (ghost_ball.entity_a.has_value()) {
        if (const Entity* const target = state.entity_manager.GetEntity(*ghost_ball.entity_a)) {
            target_position = target->pos;
        }
    }
    ghost_ball.acc = NormalizeOrZero(target_position - ghost_ball.pos) * kChaseSpeed;
}

/** generalize this to all square or rectangular entities somehow */
void StepEntityPhysicsAsGhostBall(std::size_t entity_idx, State& state, float dt) {
    common::EulerStep(entity_idx, state, dt);
}

} // namespace splonks::entities::ghost_ball

#include "entities/ghost_ball.hpp"

#include "audio.hpp"
#include "entity/archetype.hpp"
#include "entities/common/common.hpp"
#include "frame_data_id.hpp"
#include "state.hpp"

namespace splonks::entities::ghost_ball {

namespace {

} // namespace

extern const EntityArchetype kGhostBallArchetype{
    .type_ = EntityType::GhostBall,
    .size = Vec2::New(1.0F, 1.0F),
    .has_physics = true,
    .can_collide = false,
    .render_enabled = false,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Vulnerable,
    .step_logic = StepEntityLogicAsGhostBall,
    .step_physics = StepEntityPhysicsAsGhostBall,
    .entity_label_a = EntityLabel::GoToThis,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::NoSprite),
};

void StepEntityLogicAsGhostBall(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    (void)dt;
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
void StepEntityPhysicsAsGhostBall(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    common::EulerStep(entity_idx, state, dt);
}

} // namespace splonks::entities::ghost_ball

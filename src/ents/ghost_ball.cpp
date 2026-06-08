#include "ents/ghost_ball.hpp"

#include "audio.hpp"
#include "ent/spec.hpp"
#include "ents/common/common.hpp"
#include "aframe_id.hpp"
#include "player_queries.hpp"
#include "state.hpp"

namespace splonks::ents::ghost_ball {

namespace {

} // namespace

extern const EntSpec kGhostBallSpec{
    .type_ = EntType::GhostBall,
    .size = EntSpecSize(1.0F, 1.0F),
    .has_physics = true,
    .can_collide = false,
    .render_enabled = false,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::Vulnerable,
    .step_logic = StepEntLogicAsGhostBall,
    .step_physics = StepEntPhysicsAsGhostBall,
    .ent_label_a = EntLabel::GoToThis,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::NoSprite),
};

void StepEntLogicAsGhostBall(
    std::size_t ent_idx,
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
        Ent& ghost_ball = state.ents.ents[ent_idx];
        // check if a there is a target.
        // if no, target the player.
        if (!ghost_ball.ent_a.has_value()) {
            ghost_ball.ent_a = FindNearestPlayerVid(state, ghost_ball.GetCenter(), false);
        }
    }

    // try to fetch the target
    // set your acceleration towards your target vid
    Ent& ghost_ball = state.ents.ents[ent_idx];
    sim::Vec2 target_position = ghost_ball.pos;
    if (ghost_ball.ent_a.has_value()) {
        if (const Ent* const target = state.ents.GetEnt(*ghost_ball.ent_a)) {
            target_position = target->pos;
        }
    }
    ghost_ball.acc = sim::NormalizeOrZero(target_position - ghost_ball.pos) *
                     sim::ToSimScalar(kChaseSpeed);
}

/** generalize this to all square or rectangular ents somehow */
void StepEntPhysicsAsGhostBall(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    common::EulerStep(ent_idx, state, dt);
}

} // namespace splonks::ents::ghost_ball

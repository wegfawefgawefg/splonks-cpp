#include "ents/dvdlogo.hpp"

#include "ent/spec.hpp"
#include "ents/common/common.hpp"
#include "aframe_id.hpp"
#include "player_queries.hpp"
#include "stage_progression.hpp"
#include "world_query.hpp"

namespace splonks::ents::dvdlogo {

namespace {

sim::FxAABB TranslateAabb(sim::FxAABB aabb, sim::FxVec2 delta) {
    return sim::FxAABB::from_corners(aabb.tl + delta, aabb.br + delta);
}

bool WouldBlockAt(
    std::size_t ent_idx,
    sim::FxAABB target_aabb,
    const State& state,
    const Graphics& graphics
) {
    return AabbHitsBlockingWorldGeometryOrImpassableEnts(
        state,
        graphics,
        target_aabb,
        state.ents.ents[ent_idx].vid
    );
}

void StepBounceAxis(std::size_t ent_idx, State& state, const Graphics& graphics, sim::FxVec2 delta) {
    if (delta.x == sim::Scalar::zero() && delta.y == sim::Scalar::zero()) {
        return;
    }

    Ent& ent = state.ents.ents[ent_idx];
    const sim::FxAABB moved_aabb = TranslateAabb(ent.GetSimAABB(), delta);
    if (WouldBlockAt(ent_idx, moved_aabb, state, graphics)) {
        if (delta.x != sim::Scalar::zero()) {
            ent.vel.x = -ent.vel.x;
        }
        if (delta.y != sim::Scalar::zero()) {
            ent.vel.y = -ent.vel.y;
        }
        return;
    }

    ent.pos += delta;
}

void MaybeQueueTransitionOnPlayerContact(
    std::size_t ent_idx,
    State& state,
    const Graphics& graphics
) {
    if (state.pending_stage_transition.has_value()) {
        return;
    }

    Ent& ent = state.ents.ents[ent_idx];
    if (!ent.transition_target.has_value()) {
        return;
    }

    const Ent* const player = FindNearestPlayer(state, ent.GetSimCenter(), false);
    if (player == nullptr || !player->active || player->condition == EntCondition::Dead) {
        return;
    }

    const sim::FxAABB door_aabb = common::GetContactAabbForEnt(ent, graphics);
    const sim::FxAABB player_aabb = GetNearestWorldAabb(
        state.stage,
        door_aabb.center(),
        common::GetContactAabbForEnt(*player, graphics)
    );
    if (!gfxp::aabbs_intersect(door_aabb, player_aabb)) {
        return;
    }

    QueueStageTransition(state, *ent.transition_target);
}

} // namespace

extern const EntSpec kDvdLogoSpec{
    .type_ = EntType::DvdLogo,
    .size = EntSpecSize(16.0F, 16.0F),
    .health = 1,
    .has_physics = false,
    .can_collide = false,
    .can_be_picked_up = false,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Middle,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::Immune,
    .step_logic = StepEntLogicAsDvdLogo,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::Exit),
};

void StepEntLogicAsDvdLogo(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)audio;
    (void)dt;

    StepBounceAxis(
        ent_idx,
        state,
        graphics,
        sim::FxVec2{state.ents.ents[ent_idx].vel.x, sim::Scalar::zero()}
    );
    StepBounceAxis(
        ent_idx,
        state,
        graphics,
        sim::FxVec2{sim::Scalar::zero(), state.ents.ents[ent_idx].vel.y}
    );
    MaybeQueueTransitionOnPlayerContact(ent_idx, state, graphics);
}

} // namespace splonks::ents::dvdlogo

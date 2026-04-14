#include "entities/dvdlogo.hpp"

#include "entity/archetype.hpp"
#include "entities/common/common.hpp"
#include "frame_data_id.hpp"
#include "stage_progression.hpp"
#include "world_query.hpp"

namespace splonks::entities::dvdlogo {

namespace {

AABB TranslateAabb(const AABB& aabb, const Vec2& delta) {
    return AABB::New(aabb.tl + delta, aabb.br + delta);
}

bool WouldBlockAt(
    std::size_t entity_idx,
    const AABB& target_aabb,
    const State& state,
    const Graphics& graphics
) {
    return AabbHitsBlockingWorldGeometryOrImpassableEntities(
        state,
        graphics,
        target_aabb,
        state.entity_manager.entities[entity_idx].vid
    );
}

void StepBounceAxis(std::size_t entity_idx, State& state, const Graphics& graphics, const Vec2& delta) {
    if (delta.x == 0.0F && delta.y == 0.0F) {
        return;
    }

    Entity& entity = state.entity_manager.entities[entity_idx];
    const AABB moved_aabb = TranslateAabb(entity.GetAABB(), delta);
    if (WouldBlockAt(entity_idx, moved_aabb, state, graphics)) {
        if (delta.x != 0.0F) {
            entity.vel.x = -entity.vel.x;
        }
        if (delta.y != 0.0F) {
            entity.vel.y = -entity.vel.y;
        }
        return;
    }

    entity.pos += delta;
}

void MaybeQueueTransitionOnPlayerContact(
    std::size_t entity_idx,
    State& state,
    const Graphics& graphics
) {
    if (state.pending_stage_transition.has_value() || !state.player_vid.has_value()) {
        return;
    }

    Entity& entity = state.entity_manager.entities[entity_idx];
    if (!entity.transition_target.has_value()) {
        return;
    }

    const Entity* const player = state.entity_manager.GetEntity(*state.player_vid);
    if (player == nullptr || !player->active || player->condition == EntityCondition::Dead) {
        return;
    }

    const AABB door_aabb = common::GetContactAabbForEntity(entity, graphics);
    const AABB player_aabb = GetNearestWorldAabb(
        state.stage,
        entity.GetCenter(),
        common::GetContactAabbForEntity(*player, graphics)
    );
    if (!AabbsIntersect(door_aabb, player_aabb)) {
        return;
    }

    QueueStageTransition(state, *entity.transition_target);
}

} // namespace

extern const EntityArchetype kDvdLogoArchetype{
    .type_ = EntityType::DvdLogo,
    .size = Vec2::New(16.0F, 16.0F),
    .health = 1,
    .has_physics = false,
    .can_collide = false,
    .can_be_picked_up = false,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Middle,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Immune,
    .step_logic = StepEntityLogicAsDvdLogo,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::Exit),
};

void StepEntityLogicAsDvdLogo(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)audio;
    (void)dt;

    StepBounceAxis(
        entity_idx,
        state,
        graphics,
        Vec2::New(state.entity_manager.entities[entity_idx].vel.x, 0.0F)
    );
    StepBounceAxis(
        entity_idx,
        state,
        graphics,
        Vec2::New(0.0F, state.entity_manager.entities[entity_idx].vel.y)
    );
    MaybeQueueTransitionOnPlayerContact(entity_idx, state, graphics);
}

} // namespace splonks::entities::dvdlogo

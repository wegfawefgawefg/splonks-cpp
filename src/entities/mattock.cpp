#include "entities/mattock.hpp"

#include "audio.hpp"
#include "entity_archetype.hpp"
#include "entities/common.hpp"
#include "frame_data_id.hpp"

namespace splonks::entities::mattock {

extern const EntityArchetype kMattockArchetype{
    .type_ = EntityType::Mattock,
    .size = Vec2::New(16.0F, 16.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .state = EntityState::Idle,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Immune,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::Mattock),
};

void StepEntityLogicAsMattock(std::size_t entity_idx, State& state, Audio& audio) {
    (void)entity_idx;
    (void)state;
    (void)audio;
}

void StepEntityPhysicsAsMattock(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    common::ApplyGravity(entity_idx, state, dt);
    common::PrePartialEulerStep(entity_idx, state, dt);
    common::DoTileAndEntityCollisions(entity_idx, state, graphics, audio);
    common::ApplyGroundFriction(entity_idx, state);
    common::PostPartialEulerStep(entity_idx, state, dt);
}

} // namespace splonks::entities::mattock

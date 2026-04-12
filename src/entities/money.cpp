#include "entities/money.hpp"

#include "audio.hpp"
#include "entity_archetype.hpp"
#include "entities/common.hpp"
#include "frame_data_id.hpp"
#include "state.hpp"

namespace splonks::entities::money {

extern const EntityArchetype kGoldArchetype{
    .type_ = EntityType::Gold,
    .size = Vec2::New(5.0F, 5.0F),
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
    .damage_vulnerability = DamageVulnerability::CrushingOnly,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::GoldCoin),
};
extern const EntityArchetype kGoldStackArchetype{
    .type_ = EntityType::GoldStack,
    .size = Vec2::New(10.0F, 6.0F),
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
    .damage_vulnerability = DamageVulnerability::CrushingOnly,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::GoldStack),
};

void StepEntityLogicAsMoney(std::size_t entity_idx, State& state, Audio& audio) {
    (void)entity_idx;
    (void)state;
    (void)audio;
}

/** generalize this to all square or rectangular entities somehow */
void StepEntityPhysicsAsMoney(
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

} // namespace splonks::entities::money

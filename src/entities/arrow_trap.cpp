#include "entities/arrow_trap.hpp"

#include "entity_archetype.hpp"
#include "frame_data_id.hpp"

namespace splonks::entities::arrow_trap {

extern const EntityArchetype kArrowTrapArchetype{
    .type_ = EntityType::ArrowTrap,
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
    .super_state = EntitySuperState::Idle,
    .state = EntityState::Idle,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Immune,
    .alignment = Alignment::Enemy,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::ArrowTrap),
};

void StepEntityLogicAsArrowTrap(std::size_t entity_idx, State& state, Audio& audio) {
    (void)entity_idx;
    (void)state;
    (void)audio;
}

} // namespace splonks::entities::arrow_trap

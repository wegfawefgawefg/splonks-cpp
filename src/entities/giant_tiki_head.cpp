#include "entities/giant_tiki_head.hpp"

#include "entity_archetype.hpp"
#include "frame_data_id.hpp"

namespace splonks::entities::giant_tiki_head {

extern const EntityArchetype kGiantTikiHeadArchetype{
    .type_ = EntityType::GiantTikiHead,
    .size = Vec2::New(32.0F, 32.0F),
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
    .alignment = Alignment::Enemy,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::GiantTikiHead),
};

} // namespace splonks::entities::giant_tiki_head

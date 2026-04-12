#include "entities/kali_head.hpp"

#include "entity_archetype.hpp"
#include "frame_data_id.hpp"

namespace splonks::entities::kali_head {

extern const EntityArchetype kKaliHeadArchetype{
    .type_ = EntityType::KaliHead,
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
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::KaliHead),
};

} // namespace splonks::entities::kali_head

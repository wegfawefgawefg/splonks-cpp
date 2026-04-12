#include "entities/stomp_pad.hpp"

#include "entity_archetype.hpp"
#include "entities/common.hpp"
#include "frame_data_id.hpp"

namespace splonks::entities::stomp_pad {

extern const EntityArchetype kStompPadArchetype{
    .type_ = EntityType::StompPad,
    .size = Vec2::New(8.0F, 7.0F),
    .health = 1000,
    .has_physics = false,
    .can_collide = true,
    .can_be_picked_up = false,
    .impassable = false,
    .hurt_on_contact = true,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Vulnerable,
    .alignment = Alignment::Enemy,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::Pot),
};

} // namespace splonks::entities::stomp_pad

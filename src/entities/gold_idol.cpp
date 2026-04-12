#include "entities/gold_idol.hpp"

#include "entity_archetype.hpp"
#include "entity_core_types.hpp"
#include "frame_data_animator.hpp"
#include "frame_data_id.hpp"
#include "math_types.hpp"

namespace splonks::entities::gold_idol {

extern const EntityArchetype kGoldIdolArchetype{
    .type_ = EntityType::GoldIdol,
    .size = Vec2::New(12.0F, 12.0F),
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
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Immune,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::GoldIdol),
};

} // namespace splonks::entities::gold_idol

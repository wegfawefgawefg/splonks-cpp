#include "entities/spider_hang.hpp"

#include "entity_archetype.hpp"
#include "entity_core_types.hpp"
#include "frame_data_animator.hpp"
#include "frame_data_id.hpp"
#include "math_types.hpp"

namespace splonks::entities::spider_hang {

extern const EntityArchetype kSpiderHangArchetype{
    .type_ = EntityType::SpiderHang,
    .size = Vec2::New(16.0F, 16.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = false,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stunned = true,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .state = EntityState::Idle,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Vulnerable,
    .alignment = Alignment::Enemy,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::SpiderHang),
};
extern const EntityArchetype kGiantSpiderHangArchetype{
    .type_ = EntityType::GiantSpiderHang,
    .size = Vec2::New(16.0F, 16.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = false,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stunned = true,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .state = EntityState::Idle,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Vulnerable,
    .alignment = Alignment::Enemy,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::GiantSpiderHang),
};

} // namespace splonks::entities::spider_hang

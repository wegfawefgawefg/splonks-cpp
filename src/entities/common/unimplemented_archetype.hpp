#pragma once

#include "entity/archetype.hpp"
#include "frame_data_animator.hpp"
#include "frame_data_id.hpp"

namespace splonks::entities::common {

inline EntityArchetype MakeUnimplementedClassicArchetype(EntityType type_) {
    return EntityArchetype{
        .type_ = type_,
        .size = Vec2::New(16.0F, 16.0F),
        .health = 3,
        .has_physics = true,
        .can_collide = true,
        .can_be_picked_up = true,
        .can_only_be_picked_up_if_dead_or_stunned = true,
        .impassable = false,
        .hurt_on_contact = false,
        .can_be_stunned = true,
        .draw_layer = DrawLayer::Foreground,
        .facing = LeftOrRight::Left,
        .condition = EntityCondition::Normal,
        .ai_state = EntityAiState::Idle,
        .display_state = EntityDisplayState::Neutral,
        .damage_vulnerability = DamageVulnerability::Vulnerable,
        .alignment = Alignment::Neutral,
        .frame_data_animator = FrameDataAnimator::New(frame_data_ids::NoSprite),
    };
}

} // namespace splonks::entities::common

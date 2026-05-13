#pragma once

#include "ent/spec.hpp"
#include "aframe_animator.hpp"
#include "aframe_id.hpp"

namespace splonks::ents::common {

inline EntSpec MakeUnimplementedClassicSpec(EntType type_) {
    return EntSpec{
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
        .facing = Side::Left,
        .condition = EntCondition::Normal,
        .ai_state = EntAiState::Idle,
        .display_state = EntDisplayState::Neutral,
        .damage_vuln = DamageVuln::Vulnerable,
        .alignment = Alignment::Neutral,
        .aframe_animator = AFrameAnimator::New(aframe_ids::NoSprite),
    };
}

inline EntSpec MakeUnimplementedClassicNonStompableSpec(EntType type_) {
    EntSpec spec = MakeUnimplementedClassicSpec(type_);
    spec.can_be_stomped = false;
    return spec;
}

} // namespace splonks::ents::common

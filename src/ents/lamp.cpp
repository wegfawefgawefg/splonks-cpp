#include "ents/lamp.hpp"

#include "ent/spec.hpp"
#include "aframe_id.hpp"

namespace splonks::ents::lamp {

extern const EntSpec kLampSpec{
    .type_ = EntType::Lamp,
    .size = Vec2::New(16.0F, 16.0F),
    .health = 1,
    .has_physics = false,
    .can_collide = false,
    .can_be_picked_up = false,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Middle,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::Immune,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::Lantern),
};

extern const EntSpec kLampRedSpec{
    .type_ = EntType::LampRed,
    .size = Vec2::New(16.0F, 16.0F),
    .health = 1,
    .has_physics = false,
    .can_collide = false,
    .can_be_picked_up = false,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Middle,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::Immune,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::LanternRed),
};

} // namespace splonks::ents::lamp

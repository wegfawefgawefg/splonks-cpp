#include "ents/stomp_pad.hpp"

#include "ent/spec.hpp"
#include "ents/common/common.hpp"
#include "aframe_id.hpp"

namespace splonks::ents::stomp_pad {

extern const EntSpec kStompPadSpec{
    .type_ = EntType::StompPad,
    .size = Vec2::New(8.0F, 7.0F),
    .health = 1000,
    .has_physics = false,
    .can_collide = true,
    .can_be_picked_up = false,
    .impassable = false,
    .hurt_on_contact = true,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::Vulnerable,
    .alignment = Alignment::Enemy,
    .aframe_animator = AFrameAnimator::New(aframe_ids::Pot),
};

} // namespace splonks::ents::stomp_pad

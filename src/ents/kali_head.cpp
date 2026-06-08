#include "ents/kali_head.hpp"

#include "ent/spec.hpp"
#include "aframe_id.hpp"

namespace splonks::ents::kali_head {

extern const EntSpec kKaliHeadSpec{
    .type_ = EntType::KaliHead,
    .size = EntSpecSize(32.0F, 32.0F),
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
    .aframe_animator = AFrameAnimator::New(aframe_ids::KaliHead),
};

} // namespace splonks::ents::kali_head

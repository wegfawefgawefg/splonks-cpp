#include "ents/altar.hpp"

#include "ent/spec.hpp"
#include "ent/core_types.hpp"
#include "aframe_animator.hpp"
#include "aframe_id.hpp"
#include "math_types.hpp"

namespace splonks::ents::altar {

extern const EntSpec kAltarSpec{
    .type_ = EntType::Altar,
    .size = Vec2::New(16.0F, 16.0F),
    .health = 1,
    .has_physics = false,
    .can_collide = true,
    .can_be_picked_up = false,
    .impassable = true,
    .hurt_on_contact = false,
    .vanish_on_death = true,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Middle,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::ExplosionOnly,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::AltarLeft),
};

} // namespace splonks::ents::altar

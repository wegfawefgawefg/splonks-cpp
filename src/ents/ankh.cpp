#include "ents/ankh.hpp"

#include "effects.hpp"
#include "aframe_animator.hpp"
#include "aframe_id.hpp"

namespace splonks::ents::ankh {

extern const EntSpec kAnkhSpec{
    .type_ = EntType::Ankh,
    .size = Vec2::New(8.0F, 8.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::CrushingOnly,
    .proj_contact_damage_amount = 0,
    .can_apply_proj_contact = false,
    .pickup_effect = EffectId::Ankh,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::Ankh),
};

} // namespace splonks::ents::ankh

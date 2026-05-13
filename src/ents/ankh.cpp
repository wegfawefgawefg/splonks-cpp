#include "entities/ankh.hpp"

#include "effects.hpp"
#include "frame_data_animator.hpp"
#include "frame_data_id.hpp"

namespace splonks::entities::ankh {

extern const EntityArchetype kAnkhArchetype{
    .type_ = EntityType::Ankh,
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
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::CrushingOnly,
    .projectile_contact_damage_amount = 0,
    .can_apply_projectile_contact = false,
    .pickup_effect = EffectId::Ankh,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::Ankh),
};

} // namespace splonks::entities::ankh

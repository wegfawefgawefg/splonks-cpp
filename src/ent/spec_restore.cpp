#include "ent/spec_restore.hpp"

#include "ent/spec.hpp"

namespace splonks {

namespace {

const EntSpec& GetSpecForEnt(const Ent& ent) {
    return GetEntSpec(ent.type_);
}

} // namespace

void RestoreEntHasPhysicsFromSpec(Ent& ent) { ent.has_physics = GetSpecForEnt(ent).has_physics; }
void RestoreEntCanCollideFromSpec(Ent& ent) { ent.can_collide = GetSpecForEnt(ent).can_collide; }
void RestoreEntCanBePickedUpFromSpec(Ent& ent) { ent.can_be_picked_up = GetSpecForEnt(ent).can_be_picked_up; }
void RestoreEntImpassableFromSpec(Ent& ent) { ent.impassable = GetSpecForEnt(ent).impassable; }
void RestoreEntCanBeHungOnFromSpec(Ent& ent) { ent.can_be_hung_on = GetSpecForEnt(ent).can_be_hung_on; }
void RestoreEntHurtOnContactFromSpec(Ent& ent) { ent.hurt_on_contact = GetSpecForEnt(ent).hurt_on_contact; }
void RestoreEntVanishOnDeathFromSpec(Ent& ent) { ent.vanish_on_death = GetSpecForEnt(ent).vanish_on_death; }
void RestoreEntAffectedByGroundFrictionFromSpec(Ent& ent) { ent.affected_by_ground_friction = GetSpecForEnt(ent).affected_by_ground_friction; }
void RestoreEntSupportGroundFrictionFromSpec(Ent& ent) { ent.support_ground_friction = GetSpecForEnt(ent).support_ground_friction; }
void RestoreEntPushableFromSpec(Ent& ent) { ent.pushable = GetSpecForEnt(ent).pushable; }
void RestoreEntPushAccFromSpec(Ent& ent) { ent.push_acc = GetSpecForEnt(ent).push_acc; }
void RestoreEntThrowVelocityScaleFromSpec(Ent& ent) { ent.throw_velocity_scale = GetSpecForEnt(ent).throw_velocity_scale; }
void RestoreEntPickupEffectFromSpec(Ent& ent) { ent.pickup_effect = GetSpecForEnt(ent).pickup_effect; }
void RestoreEntBuyableFromSpec(Ent& ent) { ent.buyable = GetSpecForEnt(ent).buyable; }
void RestoreEntDamageAnimFromSpec(Ent& ent) { ent.damage_anim = GetSpecForEnt(ent).damage_anim; }
void RestoreEntDamageSoundFromSpec(Ent& ent) { ent.damage_sound = GetSpecForEnt(ent).damage_sound; }
void RestoreEntCollideSoundFromSpec(Ent& ent) { ent.collide_sound = GetSpecForEnt(ent).collide_sound; }
void RestoreEntDeathAudioAssetIdFromSpec(Ent& ent) { ent.death_sound = GetSpecForEnt(ent).death_sound; }
void RestoreEntOnDeathFromSpec(Ent& ent) { ent.on_death = GetSpecForEnt(ent).on_death; }
void RestoreEntOnDamageFromSpec(Ent& ent) { ent.on_damage = GetSpecForEnt(ent).on_damage; }
void RestoreEntOnUseFromSpec(Ent& ent) { ent.on_use = GetSpecForEnt(ent).on_use; }
void RestoreEntControlLogicFromSpec(Ent& ent) { ent.control_logic = GetSpecForEnt(ent).control_logic; }
void RestoreEntStepLogicFromSpec(Ent& ent) { ent.step_logic = GetSpecForEnt(ent).step_logic; }
void RestoreEntStepPhysicsFromSpec(Ent& ent) { ent.step_physics = GetSpecForEnt(ent).step_physics; }
void RestoreEntCrusherPusherFromSpec(Ent& ent) { ent.crusher_pusher = GetSpecForEnt(ent).crusher_pusher; }
void RestoreEntCanGoOnBackFromSpec(Ent& ent) { ent.can_go_on_back = GetSpecForEnt(ent).can_go_on_back; }
void RestoreEntCanHangLedgeFromSpec(Ent& ent) { ent.can_hang_ledge = GetSpecForEnt(ent).can_hang_ledge; }
void RestoreEntCanBeStunnedFromSpec(Ent& ent) { ent.can_be_stunned = GetSpecForEnt(ent).can_be_stunned; }
void RestoreEntStunRecoversOnGroundFromSpec(Ent& ent) { ent.stun_recovers_on_ground = GetSpecForEnt(ent).stun_recovers_on_ground; }
void RestoreEntStunRecoversWhileHeldFromSpec(Ent& ent) { ent.stun_recovers_while_held = GetSpecForEnt(ent).stun_recovers_while_held; }
void RestoreEntSizeFromSpec(Ent& ent) { ent.size = GetSpecForEnt(ent).size; }
void RestoreEntFacingFromSpec(Ent& ent) { ent.facing = GetSpecForEnt(ent).facing; }
void RestoreEntDrawLayerFromSpec(Ent& ent) { ent.draw_layer = GetSpecForEnt(ent).draw_layer; }
void RestoreEntRenderEnabledFromSpec(Ent& ent) { ent.render_enabled = GetSpecForEnt(ent).render_enabled; }
void RestoreEntConditionFromSpec(Ent& ent) { ent.condition = GetSpecForEnt(ent).condition; }
void RestoreEntAiStateFromSpec(Ent& ent) { ent.ai_state = GetSpecForEnt(ent).ai_state; }
void RestoreEntHealthFromSpec(Ent& ent) { ent.health = GetSpecForEnt(ent).health; }
void RestoreEntCounterAFromSpec(Ent& ent) { ent.counter_a = GetSpecForEnt(ent).counter_a; }
void RestoreEntCounterBFromSpec(Ent& ent) { ent.counter_b = GetSpecForEnt(ent).counter_b; }
void RestoreEntDamageVulnFromSpec(Ent& ent) { ent.damage_vuln = GetSpecForEnt(ent).damage_vuln; }
void RestoreEntLabelAFromSpec(Ent& ent) { ent.ent_label_a = GetSpecForEnt(ent).ent_label_a; }
void RestoreEntAlignmentFromSpec(Ent& ent) { ent.alignment = GetSpecForEnt(ent).alignment; }
void RestoreEntAFrameAnimatorFromSpec(Ent& ent) { ent.aframe_animator = GetSpecForEnt(ent).aframe_animator; }

} // namespace splonks

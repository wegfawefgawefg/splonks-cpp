#include "entity/archetype_restore.hpp"

#include "entity/archetype.hpp"

namespace splonks {

namespace {

const EntityArchetype& GetArchetypeForEntity(const Entity& entity) {
    return GetEntityArchetype(entity.type_);
}

} // namespace

void RestoreEntityHasPhysicsFromArchetype(Entity& entity) { entity.has_physics = GetArchetypeForEntity(entity).has_physics; }
void RestoreEntityCanCollideFromArchetype(Entity& entity) { entity.can_collide = GetArchetypeForEntity(entity).can_collide; }
void RestoreEntityCanBePickedUpFromArchetype(Entity& entity) { entity.can_be_picked_up = GetArchetypeForEntity(entity).can_be_picked_up; }
void RestoreEntityImpassableFromArchetype(Entity& entity) { entity.impassable = GetArchetypeForEntity(entity).impassable; }
void RestoreEntityCanBeHungOnFromArchetype(Entity& entity) { entity.can_be_hung_on = GetArchetypeForEntity(entity).can_be_hung_on; }
void RestoreEntityHurtOnContactFromArchetype(Entity& entity) { entity.hurt_on_contact = GetArchetypeForEntity(entity).hurt_on_contact; }
void RestoreEntityVanishOnDeathFromArchetype(Entity& entity) { entity.vanish_on_death = GetArchetypeForEntity(entity).vanish_on_death; }
void RestoreEntityAffectedByGroundFrictionFromArchetype(Entity& entity) { entity.affected_by_ground_friction = GetArchetypeForEntity(entity).affected_by_ground_friction; }
void RestoreEntitySupportGroundFrictionFromArchetype(Entity& entity) { entity.support_ground_friction = GetArchetypeForEntity(entity).support_ground_friction; }
void RestoreEntityPassiveItemFromArchetype(Entity& entity) { entity.passive_item = GetArchetypeForEntity(entity).passive_item; }
void RestoreEntityBuyableFromArchetype(Entity& entity) { entity.buyable = GetArchetypeForEntity(entity).buyable; }
void RestoreEntityDamageAnimationFromArchetype(Entity& entity) { entity.damage_animation = GetArchetypeForEntity(entity).damage_animation; }
void RestoreEntityDamageSoundFromArchetype(Entity& entity) { entity.damage_sound = GetArchetypeForEntity(entity).damage_sound; }
void RestoreEntityCollideSoundFromArchetype(Entity& entity) { entity.collide_sound = GetArchetypeForEntity(entity).collide_sound; }
void RestoreEntityDeathSoundEffectFromArchetype(Entity& entity) { entity.death_sound_effect = GetArchetypeForEntity(entity).death_sound_effect; }
void RestoreEntityOnDeathFromArchetype(Entity& entity) { entity.on_death = GetArchetypeForEntity(entity).on_death; }
void RestoreEntityOnDamageFromArchetype(Entity& entity) { entity.on_damage = GetArchetypeForEntity(entity).on_damage; }
void RestoreEntityOnUseFromArchetype(Entity& entity) { entity.on_use = GetArchetypeForEntity(entity).on_use; }
void RestoreEntityStepLogicFromArchetype(Entity& entity) { entity.step_logic = GetArchetypeForEntity(entity).step_logic; }
void RestoreEntityStepPhysicsFromArchetype(Entity& entity) { entity.step_physics = GetArchetypeForEntity(entity).step_physics; }
void RestoreEntityCrusherPusherFromArchetype(Entity& entity) { entity.crusher_pusher = GetArchetypeForEntity(entity).crusher_pusher; }
void RestoreEntityCanGoOnBackFromArchetype(Entity& entity) { entity.can_go_on_back = GetArchetypeForEntity(entity).can_go_on_back; }
void RestoreEntityCanHangLedgeFromArchetype(Entity& entity) { entity.can_hang_ledge = GetArchetypeForEntity(entity).can_hang_ledge; }
void RestoreEntityCanBeStunnedFromArchetype(Entity& entity) { entity.can_be_stunned = GetArchetypeForEntity(entity).can_be_stunned; }
void RestoreEntityStunRecoversOnGroundFromArchetype(Entity& entity) { entity.stun_recovers_on_ground = GetArchetypeForEntity(entity).stun_recovers_on_ground; }
void RestoreEntityStunRecoversWhileHeldFromArchetype(Entity& entity) { entity.stun_recovers_while_held = GetArchetypeForEntity(entity).stun_recovers_while_held; }
void RestoreEntitySizeFromArchetype(Entity& entity) { entity.size = GetArchetypeForEntity(entity).size; }
void RestoreEntityFacingFromArchetype(Entity& entity) { entity.facing = GetArchetypeForEntity(entity).facing; }
void RestoreEntityDrawLayerFromArchetype(Entity& entity) { entity.draw_layer = GetArchetypeForEntity(entity).draw_layer; }
void RestoreEntityRenderEnabledFromArchetype(Entity& entity) { entity.render_enabled = GetArchetypeForEntity(entity).render_enabled; }
void RestoreEntityConditionFromArchetype(Entity& entity) { entity.condition = GetArchetypeForEntity(entity).condition; }
void RestoreEntityAiStateFromArchetype(Entity& entity) { entity.ai_state = GetArchetypeForEntity(entity).ai_state; }
void RestoreEntityHealthFromArchetype(Entity& entity) { entity.health = GetArchetypeForEntity(entity).health; }
void RestoreEntityBombsFromArchetype(Entity& entity) { entity.bombs = GetArchetypeForEntity(entity).bombs; }
void RestoreEntityRopesFromArchetype(Entity& entity) { entity.ropes = GetArchetypeForEntity(entity).ropes; }
void RestoreEntityCounterAFromArchetype(Entity& entity) { entity.counter_a = GetArchetypeForEntity(entity).counter_a; }
void RestoreEntityCounterBFromArchetype(Entity& entity) { entity.counter_b = GetArchetypeForEntity(entity).counter_b; }
void RestoreEntityDamageVulnerabilityFromArchetype(Entity& entity) { entity.damage_vulnerability = GetArchetypeForEntity(entity).damage_vulnerability; }
void RestoreEntityLabelAFromArchetype(Entity& entity) { entity.entity_label_a = GetArchetypeForEntity(entity).entity_label_a; }
void RestoreEntityAlignmentFromArchetype(Entity& entity) { entity.alignment = GetArchetypeForEntity(entity).alignment; }
void RestoreEntityFrameDataAnimatorFromArchetype(Entity& entity) { entity.frame_data_animator = GetArchetypeForEntity(entity).frame_data_animator; }

} // namespace splonks

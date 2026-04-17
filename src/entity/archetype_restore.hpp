#pragma once

#include "entity.hpp"

namespace splonks {

void RestoreEntityHasPhysicsFromArchetype(Entity& entity);
void RestoreEntityCanCollideFromArchetype(Entity& entity);
void RestoreEntityCanBePickedUpFromArchetype(Entity& entity);
void RestoreEntityImpassableFromArchetype(Entity& entity);
void RestoreEntityHurtOnContactFromArchetype(Entity& entity);
void RestoreEntityVanishOnDeathFromArchetype(Entity& entity);
void RestoreEntityHasGroundFrictionFromArchetype(Entity& entity);
void RestoreEntityPassiveItemFromArchetype(Entity& entity);
void RestoreEntityBuyableFromArchetype(Entity& entity);
void RestoreEntityDamageAnimationFromArchetype(Entity& entity);
void RestoreEntityDamageSoundFromArchetype(Entity& entity);
void RestoreEntityCollideSoundFromArchetype(Entity& entity);
void RestoreEntityDeathSoundEffectFromArchetype(Entity& entity);
void RestoreEntityOnDeathFromArchetype(Entity& entity);
void RestoreEntityOnDamageFromArchetype(Entity& entity);
void RestoreEntityOnUseFromArchetype(Entity& entity);
void RestoreEntityStepLogicFromArchetype(Entity& entity);
void RestoreEntityStepPhysicsFromArchetype(Entity& entity);
void RestoreEntityCrusherPusherFromArchetype(Entity& entity);
void RestoreEntityCanGoOnBackFromArchetype(Entity& entity);
void RestoreEntityCanHangLedgeFromArchetype(Entity& entity);
void RestoreEntityCanBeStunnedFromArchetype(Entity& entity);
void RestoreEntityStunRecoversOnGroundFromArchetype(Entity& entity);
void RestoreEntityStunRecoversWhileHeldFromArchetype(Entity& entity);
void RestoreEntitySizeFromArchetype(Entity& entity);
void RestoreEntityFacingFromArchetype(Entity& entity);
void RestoreEntityDrawLayerFromArchetype(Entity& entity);
void RestoreEntityRenderEnabledFromArchetype(Entity& entity);
void RestoreEntityConditionFromArchetype(Entity& entity);
void RestoreEntityAiStateFromArchetype(Entity& entity);
void RestoreEntityHealthFromArchetype(Entity& entity);
void RestoreEntityBombsFromArchetype(Entity& entity);
void RestoreEntityRopesFromArchetype(Entity& entity);
void RestoreEntityCounterAFromArchetype(Entity& entity);
void RestoreEntityCounterBFromArchetype(Entity& entity);
void RestoreEntityDamageVulnerabilityFromArchetype(Entity& entity);
void RestoreEntityLabelAFromArchetype(Entity& entity);
void RestoreEntityAlignmentFromArchetype(Entity& entity);
void RestoreEntityFrameDataAnimatorFromArchetype(Entity& entity);

} // namespace splonks

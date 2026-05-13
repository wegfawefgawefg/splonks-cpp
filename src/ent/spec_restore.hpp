#pragma once

#include "ent.hpp"

namespace splonks {

void RestoreEntHasPhysicsFromSpec(Ent& ent);
void RestoreEntCanCollideFromSpec(Ent& ent);
void RestoreEntCanBePickedUpFromSpec(Ent& ent);
void RestoreEntImpassableFromSpec(Ent& ent);
void RestoreEntCanBeHungOnFromSpec(Ent& ent);
void RestoreEntHurtOnContactFromSpec(Ent& ent);
void RestoreEntVanishOnDeathFromSpec(Ent& ent);
void RestoreEntAffectedByGroundFrictionFromSpec(Ent& ent);
void RestoreEntSupportGroundFrictionFromSpec(Ent& ent);
void RestoreEntPushableFromSpec(Ent& ent);
void RestoreEntPushAccFromSpec(Ent& ent);
void RestoreEntThrowVelocityScaleFromSpec(Ent& ent);
void RestoreEntPickupEffectFromSpec(Ent& ent);
void RestoreEntBuyableFromSpec(Ent& ent);
void RestoreEntDamageAnimFromSpec(Ent& ent);
void RestoreEntDamageSoundFromSpec(Ent& ent);
void RestoreEntCollideSoundFromSpec(Ent& ent);
void RestoreEntDeathAudioAssetIdFromSpec(Ent& ent);
void RestoreEntOnDeathFromSpec(Ent& ent);
void RestoreEntOnDamageFromSpec(Ent& ent);
void RestoreEntOnUseFromSpec(Ent& ent);
void RestoreEntControlLogicFromSpec(Ent& ent);
void RestoreEntStepLogicFromSpec(Ent& ent);
void RestoreEntStepPhysicsFromSpec(Ent& ent);
void RestoreEntCrusherPusherFromSpec(Ent& ent);
void RestoreEntCanGoOnBackFromSpec(Ent& ent);
void RestoreEntCanHangLedgeFromSpec(Ent& ent);
void RestoreEntCanBeStunnedFromSpec(Ent& ent);
void RestoreEntStunRecoversOnGroundFromSpec(Ent& ent);
void RestoreEntStunRecoversWhileHeldFromSpec(Ent& ent);
void RestoreEntSizeFromSpec(Ent& ent);
void RestoreEntFacingFromSpec(Ent& ent);
void RestoreEntDrawLayerFromSpec(Ent& ent);
void RestoreEntRenderEnabledFromSpec(Ent& ent);
void RestoreEntConditionFromSpec(Ent& ent);
void RestoreEntAiStateFromSpec(Ent& ent);
void RestoreEntHealthFromSpec(Ent& ent);
void RestoreEntCounterAFromSpec(Ent& ent);
void RestoreEntCounterBFromSpec(Ent& ent);
void RestoreEntDamageVulnFromSpec(Ent& ent);
void RestoreEntLabelAFromSpec(Ent& ent);
void RestoreEntAlignmentFromSpec(Ent& ent);
void RestoreEntAFrameAnimatorFromSpec(Ent& ent);

} // namespace splonks

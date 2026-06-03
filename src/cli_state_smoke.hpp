#pragma once

namespace splonks {

bool CheckStateFingerprintSmoke();
bool CheckStateEqualitySmoke();
bool CheckGameplaySnapshotCallbackRebindSmoke();
bool CheckDetReplaySmoke();
bool CheckNetworkFreshReloadOwnershipSmoke();
bool CheckInputLockstepSmoke();
bool CheckJoinBarrierNextStageRestartSmoke();

} // namespace splonks

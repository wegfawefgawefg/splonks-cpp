#pragma once

#include "network/net_replication_payloads.hpp"
#include "presentation_commands.hpp"

namespace splonks {

struct State;

namespace network {

void ReplicateEntitySpawned(State& state, const GameplayEntitySpawned& spawned);
void ReplicateEntityDeactivated(State& state, const GameplayEntityDeactivated& deactivated);
void ReplicateEntityHeld(State& state, const GameplayEntityHeld& held);
void ReplicateEntityDropped(State& state, const GameplayEntityDropped& dropped);
void ReplicateEntityThrown(State& state, const GameplayEntityThrown& thrown);
void ReplicateEntityDamaged(State& state, const GameplayEntityDamaged& damaged);
void ReplicateEntityStatePatched(State& state, const GameplayEntityStatePatched& patched);
void ReplicatePlayerStatePatched(State& state, const GameplayPlayerStatePatched& patched);
void ReplicateRunStatePatched(State& state, bool include_snapshot_fingerprint = false);
void ReplicateTileChanged(State& state, const GameplayTileChanged& changed);
void ReplicateTileBroken(State& state, const GameplayTileBroken& broken);
void ReplicateStageLightAdded(State& state, const GameplayStageLightAdded& added);
void ReplicateStageLightRemoved(State& state, const GameplayStageLightRemoved& removed);
void ReplicatePresentationCommand(State& state, const PresentationCommand& command);

} // namespace network
} // namespace splonks

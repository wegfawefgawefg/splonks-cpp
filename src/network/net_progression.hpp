#pragma once

#include "network/net_protocol.hpp"
#include "network/net_transport.hpp"
#include "stage.hpp"

namespace splonks {

struct Graphics;
struct State;

namespace network {

void SendStageSyncToAllRemotes(State& state, NetTransportRuntime& transport);
bool SendPendingStageTransitionSyncToAllRemotes(State& state, NetTransportRuntime& transport);
void ApplyStageSync(
    State& state,
    const Graphics& graphics,
    NetTransportRuntime& transport,
    const StageSyncPacket& packet
);
bool ApplyPendingPeerStageSync(State& state, const Graphics& graphics);
void NotifyStageLoaded(State& state);

} // namespace network
} // namespace splonks

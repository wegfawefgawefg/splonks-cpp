#pragma once

#include "graphics.hpp"
#include "network/net_protocol.hpp"
#include "network/net_transport.hpp"
#include "player_id.hpp"
#include "state.hpp"

#include <string>
#include <vector>

namespace splonks::network {

void SendEncodedPacket(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const EncodedNetPacket& encoded
);

bool IsReplicatedEntityStateEvent(const NetEvent& event);
bool IsReplicatedTileEvent(const NetEvent& event);
bool IsReplicatedFluidCellEvent(const NetEvent& event);
bool IsReplicatedEntitySpawnedEvent(const NetEvent& event);
bool IsReplicatedEntityDamageEvent(const NetEvent& event);
bool IsReplicatedActionRequestEvent(const NetEvent& event);
bool IsReplicatedEntityCarryEvent(const NetEvent& event);
bool IsReplicatedEntityLifecycleEvent(const NetEvent& event);
bool IsReplicatedPlayerStateEvent(const NetEvent& event);
bool IsReplicatedRunStateEvent(const NetEvent& event);
bool IsReplicatedPresentationCommandEvent(const NetEvent& event);

TileEventEntry MakeTileEventEntry(const NetEvent& event);
FluidCellEventEntry MakeFluidCellEventEntry(const NetEvent& event);
EntitySpawnedEventEntry MakeEntitySpawnedEventEntry(const NetEvent& event);
EntityDamageEventEntry MakeEntityDamageEventEntry(const NetEvent& event);
EntityStateEventEntry MakeEntityStateEventEntry(const NetEvent& event);
EntityCarryEventEntry MakeEntityCarryEventEntry(const NetEvent& event);
EntityLifecycleEventEntry MakeEntityLifecycleEventEntry(const NetEvent& event);
PlayerStateEventEntry MakePlayerStateEventEntry(const NetEvent& event);
RunStateEventEntry MakeRunStateEventEntry(const NetEvent& event);
PresentationCommandEventEntry MakePresentationCommandEventEntry(const NetEvent& event);
ActionRequestEventEntry MakeActionRequestEventEntry(const NetEvent& event);

NetEvent MakeTileEvent(const TileEventEntry& entry);
NetEvent MakeFluidCellEvent(const FluidCellEventEntry& entry);
NetEvent MakeEntitySpawnedEvent(const EntitySpawnedEventEntry& entry);
NetEvent MakeEntityDamageEvent(const EntityDamageEventEntry& entry);
NetEvent MakeEntityStateEvent(const EntityStateEventEntry& entry);
NetEvent MakeEntityCarryEvent(const EntityCarryEventEntry& entry);
NetEvent MakeEntityLifecycleEvent(const EntityLifecycleEventEntry& entry);
NetEvent MakePlayerStateEvent(const PlayerStateEventEntry& entry);
NetEvent MakeRunStateEvent(const RunStateEventEntry& entry);
NetEvent MakePresentationCommandEvent(const PresentationCommandEventEntry& entry);
NetEvent MakeActionRequestEvent(const ActionRequestEventEntry& entry);

void SendTileEvents(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetEvent>& events
);
void SendFluidCellEvents(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetEvent>& events
);
void SendEntitySpawnedEvents(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetEvent>& events
);
void SendEntityDamageEvents(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetEvent>& events
);
void SendEntityStateEvents(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetEvent>& events
);
void SendEntityCarryEvents(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetEvent>& events
);
void SendEntityLifecycleEvents(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetEvent>& events
);
void SendPlayerStateEvents(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetEvent>& events
);
void SendRunStateEvents(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetEvent>& events
);
void SendPresentationCommandEvents(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetEvent>& events
);
void SendActionRequestEvents(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetEvent>& events
);
void SendActionRequestAck(
    State& state,
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetEventId>& event_ids
);

void SendPendingPeerEventsToCoordinator(State& state, NetTransportRuntime& transport);
void SendOrderedEventsToAllRemotes(State& state, NetTransportRuntime& transport);
void PruneAckedOrderedEvents(State& state, const NetTransportRuntime& transport);
void SendDurableEventAckToCoordinator(State& state, NetTransportRuntime& transport);
void HandleDurableEventAckAsCoordinator(
    State& state,
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const DurableEventAckPacket& ack
);
void HandleTileEventsAsPeer(State& state, const TileEventsPacket& packet);
void HandleFluidCellEventsAsPeer(State& state, const FluidCellEventsPacket& packet);
void HandleEntitySpawnedEventsAsPeer(State& state, const EntitySpawnedEventsPacket& packet);
void HandleEntityDamageEventsAsPeer(State& state, const EntityDamageEventsPacket& packet);
void HandleEntityStateEventsAsPeer(State& state, const EntityStateEventsPacket& packet);
void HandleEntityCarryEventsAsPeer(State& state, const EntityCarryEventsPacket& packet);
void HandleEntityLifecycleEventsAsPeer(State& state, const EntityLifecycleEventsPacket& packet);
void HandlePlayerStateEventsAsPeer(State& state, const PlayerStateEventsPacket& packet);
void HandleRunStateEventsAsPeer(State& state, const RunStateEventsPacket& packet);
void HandlePresentationCommandEventsAsCoordinator(
    State& state,
    const PresentationCommandEventsPacket& packet
);
void HandlePresentationCommandEventsAsPeer(State& state, const PresentationCommandEventsPacket& packet);
void HandleActionRequestEventsAsCoordinator(
    State& state,
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const ActionRequestEventsPacket& packet
);
void HandleActionRequestAckAsPeer(State& state, const ActionRequestAckPacket& packet);

void HandleJoinRequestAsCoordinator(
    State& state,
    const Graphics& graphics,
    NetTransportRuntime& transport,
    const UdpPacket& udp_packet,
    const JoinRequestPacket& request
);
void HandleJoinAcceptAsPeer(
    State& state,
    const Graphics& graphics,
    NetTransportRuntime& transport,
    const JoinAcceptPacket& accept
);
void HandleLeaveNoticeAsCoordinator(
    State& state,
    NetTransportRuntime& transport,
    const LeaveNoticePacket& leave
);

void EnsureSpawnedPlayer(
    State& state,
    PlayerId player_id,
    bool local,
    bool primary,
    const Vec2& pos,
    const Graphics& graphics
);

void SendSnapshotsToEndpoint(
    State& state,
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint
);
void SendSnapshotsToAllRemotes(State& state, NetTransportRuntime& transport);
void RelaySnapshotsToOtherRemotes(
    NetTransportRuntime& transport,
    const NetEndpoint& source_endpoint,
    const PlayerSnapshotsPacket& snapshots
);
void ApplyPlayerSnapshots(
    State& state,
    const Graphics& graphics,
    NetTransportRuntime& transport,
    const PlayerSnapshotsPacket& packet
);
void StepRemotePlayerInterpolation(
    State& state,
    NetTransportRuntime& transport,
    const Graphics& graphics
);
void SyncNetworkAttachmentsAfterRemoteMovement(State& state, const Graphics& graphics);
bool ShouldSendSnapshots(const State& state, const NetTransportRuntime& transport);

void SendReplicatedEntityStatePatchesToAllRemotes(State& state, NetTransportRuntime& transport);
void SendCoordinatorEntityRepairPatchesToAllRemotes(State& state, NetTransportRuntime& transport);
void SendReplicatedFluidCellPatchesToAllRemotes(State& state, NetTransportRuntime& transport);

} // namespace splonks::network

#pragma once

#include "graphics.hpp"
#include "network/net_protocol.hpp"
#include "network/net_transport.hpp"
#include "player_id.hpp"
#include "state.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace splonks::network {

Vec2 GetPrimaryPlayerSpawnPos(const State& state);
Vec2 GetRemoteSpawnPos(const State& state);
Vec2 GetEntranceOrRemoteSpawnPos(const State& state);
const NetRetainedPlayerState* FindRetainedPlayerState(const State& state, PlayerId player_id);
void RemoveRetainedPlayerState(State& state, PlayerId player_id);
void StoreRetainedPlayerState(State& state, const PlayerSlot& slot, const Entity& player);
void CleanupExpiredRetainedPlayerStates(State& state);
void DeactivateRetainedAttachedEntity(
    State& state,
    const NetRetainedAttachedEntityState& retained,
    std::optional<VID> attached_vid
);
bool IsRetainedReconnectMode(NetReconnectSpawnMode mode);
Vec2 ResolveReconnectSpawnPos(
    const State& state,
    const NetRetainedPlayerState* retained,
    std::size_t player_index
);
void ApplyRetainedPlayerState(
    State& state,
    PlayerId player_id,
    const NetRetainedPlayerState& retained,
    const Vec2& spawn_pos,
    const Graphics& graphics
);

void SendEncodedPacket(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const EncodedNetPacket& encoded
);

bool IsReplicatedEntityStateMessage(const NetMessage& message);
bool IsReplicatedTileMessage(const NetMessage& message);
bool IsReplicatedFluidCellMessage(const NetMessage& message);
bool IsReplicatedEntitySpawnedMessage(const NetMessage& message);
bool IsReplicatedEntityDamageMessage(const NetMessage& message);
bool IsReplicatedActionRequestMessage(const NetMessage& message);
bool IsReplicatedEntityCarryMessage(const NetMessage& message);
bool IsReplicatedEntityLifecycleMessage(const NetMessage& message);
bool IsReplicatedPlayerStateMessage(const NetMessage& message);
bool IsReplicatedRunStateMessage(const NetMessage& message);
bool IsReplicatedPresentationCommandMessage(const NetMessage& message);

TileMessageEntry MakeTileMessageEntry(const NetMessage& message);
FluidCellMessageEntry MakeFluidCellMessageEntry(const NetMessage& message);
EntitySpawnedMessageEntry MakeEntitySpawnedMessageEntry(const NetMessage& message);
EntityDamageMessageEntry MakeEntityDamageMessageEntry(const NetMessage& message);
EntityStateMessageEntry MakeEntityStateMessageEntry(const NetMessage& message);
EntityCarryMessageEntry MakeEntityCarryMessageEntry(const NetMessage& message);
EntityLifecycleMessageEntry MakeEntityLifecycleMessageEntry(const NetMessage& message);
PlayerStateMessageEntry MakePlayerStateMessageEntry(const NetMessage& message);
RunStateMessageEntry MakeRunStateMessageEntry(const NetMessage& message);
PresentationCommandMessageEntry MakePresentationCommandMessageEntry(const NetMessage& message);
ActionRequestMessageEntry MakeActionRequestMessageEntry(const NetMessage& message);

NetMessage MakeTileMessage(const TileMessageEntry& entry);
NetMessage MakeFluidCellMessage(const FluidCellMessageEntry& entry);
NetMessage MakeEntitySpawnedMessage(const EntitySpawnedMessageEntry& entry);
NetMessage MakeEntityDamageMessage(const EntityDamageMessageEntry& entry);
NetMessage MakeEntityStateMessage(const EntityStateMessageEntry& entry);
NetMessage MakeEntityCarryMessage(const EntityCarryMessageEntry& entry);
NetMessage MakeEntityLifecycleMessage(const EntityLifecycleMessageEntry& entry);
NetMessage MakePlayerStateMessage(const PlayerStateMessageEntry& entry);
NetMessage MakeRunStateMessage(const RunStateMessageEntry& entry);
NetMessage MakePresentationCommandMessage(const PresentationCommandMessageEntry& entry);
NetMessage MakeActionRequestMessage(const ActionRequestMessageEntry& entry);

void SendTileMessages(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetMessage>& messages
);
void SendFluidCellMessages(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetMessage>& messages
);
void SendEntitySpawnedMessages(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetMessage>& messages
);
void SendEntityDamageMessages(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetMessage>& messages
);
void SendEntityStateMessages(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetMessage>& messages
);
void SendEntityCarryMessages(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetMessage>& messages
);
void SendEntityLifecycleMessages(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetMessage>& messages
);
void SendPlayerStateMessages(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetMessage>& messages
);
void SendRunStateMessages(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetMessage>& messages
);
void SendPresentationCommandMessages(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetMessage>& messages
);
void SendActionRequestMessages(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetMessage>& messages
);
void SendActionRequestAck(
    State& state,
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetMessageId>& message_ids
);

void SendJoinRequest(State& state);
void SendLeaveNotice(State& state);
bool EnsureHostSyncedStage(State& state, std::string* status_out);
void MarkRemoteEndpointHeard(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    std::uint64_t frame
);
void RemoveRemotePlayers(
    State& state,
    NetTransportRuntime& transport,
    const std::vector<PlayerId>& player_ids
);
void CleanupTimedOutRemoteEndpoints(State& state, NetTransportRuntime& transport);
void StepHostPackets(State& state, const Graphics& graphics, NetTransportRuntime& transport);
void StepPeerPackets(State& state, const Graphics& graphics, NetTransportRuntime& transport);

void SendPendingPeerMessagesToCoordinator(State& state, NetTransportRuntime& transport);
void SendOrderedMessagesToAllRemotes(State& state, NetTransportRuntime& transport);
void PruneAckedOrderedMessages(State& state, const NetTransportRuntime& transport);
void SendDurableMessageAckToCoordinator(State& state, NetTransportRuntime& transport);
void HandleDurableMessageAckAsCoordinator(
    State& state,
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const DurableMessageAckPacket& ack
);
void HandleTileMessagesAsPeer(State& state, const TileMessagesPacket& packet);
void HandleFluidCellMessagesAsPeer(State& state, const FluidCellMessagesPacket& packet);
void HandleEntitySpawnedMessagesAsPeer(State& state, const EntitySpawnedMessagesPacket& packet);
void HandleEntityDamageMessagesAsPeer(State& state, const EntityDamageMessagesPacket& packet);
void HandleEntityStateMessagesAsPeer(State& state, const EntityStateMessagesPacket& packet);
void HandleEntityCarryMessagesAsPeer(State& state, const EntityCarryMessagesPacket& packet);
void HandleEntityLifecycleMessagesAsPeer(State& state, const EntityLifecycleMessagesPacket& packet);
void HandlePlayerStateMessagesAsPeer(State& state, const PlayerStateMessagesPacket& packet);
void HandleRunStateMessagesAsPeer(State& state, const RunStateMessagesPacket& packet);
void HandlePresentationCommandMessagesAsCoordinator(
    State& state,
    const PresentationCommandMessagesPacket& packet
);
void HandlePresentationCommandMessagesAsPeer(State& state, const PresentationCommandMessagesPacket& packet);
void HandleActionRequestMessagesAsCoordinator(
    State& state,
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const ActionRequestMessagesPacket& packet
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

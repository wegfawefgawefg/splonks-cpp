#pragma once

#include "audio.hpp"
#include "graphics.hpp"
#include "network/net_protocol.hpp"
#include "network/net_transport.hpp"
#include "player_id.hpp"
#include "state.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace splonks::network {

sim::FxVec2 GetPrimaryPlayerSpawnPos(const State& state);
sim::FxVec2 GetRemoteSpawnPos(const State& state);
sim::FxVec2 GetEntranceOrRemoteSpawnPos(const State& state);
const NetRetainedPlayerState* FindRetainedPlayerState(const State& state, PlayerId player_id);
void RemoveRetainedPlayerState(State& state, PlayerId player_id);
void StoreRetainedPlayerState(State& state, const PlayerSlot& slot, const Ent& player);
void CleanupExpiredRetainedPlayerStates(State& state);
void DeactivateRetainedAttachedEnt(
    State& state,
    const NetRetainedAttachedEntState& retained,
    std::optional<VID> attached_vid
);
bool IsRetainedReconnectMode(NetReconnectSpawnMode mode);
sim::FxVec2 ResolveReconnectSpawnPos(
    const State& state,
    const NetRetainedPlayerState* retained,
    std::size_t player_index
);
void ApplyRetainedPlayerState(
    State& state,
    PlayerId player_id,
    const NetRetainedPlayerState& retained,
    sim::FxVec2 spawn_pos,
    const Graphics& graphics
);

void SendEncodedPacket(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const EncodedNetPacket& encoded
);
void FlushFuzzedOutgoingPackets(NetTransportRuntime& transport);
void MaintainRealnetPunch(State& state, NetTransportRuntime& transport);
void MaintainRealnetRelay(State& state, NetTransportRuntime& transport);
bool WrapRealnetRelayPacket(
    NetTransportRuntime& transport,
    const UdpPacket& packet,
    UdpPacket& out
);
bool TryHandleRealnetPunchPacket(
    State& state,
    NetTransportRuntime& transport,
    const UdpPacket& packet
);
bool TryHandleRealnetRelayPacket(
    State& state,
    NetTransportRuntime& transport,
    const UdpPacket& packet,
    UdpPacket& unwrapped
);

void SendJoinRequest(State& state);
void SendLeaveNotice(State& state);
void DrainPendingJoinRequestsAsHost(State& state, const Graphics& graphics, NetTransportRuntime& transport);
void HandleJoinPendingAsPeer(
    State& state,
    NetTransportRuntime& transport,
    const JoinPendingPacket& pending
);
bool IsInputLockstepSession(const State& state);
bool IsInputLockstepActive(const State& state);
bool IsInputLockstepCatchupBlocking(const State& state);
void ResetInputLockstepState(State& state);
bool PrepareInputLockstepFrame(State& state, Graphics& graphics);
void MaintainInputLockstepTransport(State& state, Graphics& graphics);
bool ReplayPendingInputLockstepRollback(State& state, Graphics& graphics);
void HandleInputFrameRecords(State& state, const InputFrameRecordsPacket& packet);
void HandleLockstepSettingsPacket(State& state, const LockstepSettingsPacket& packet);
void HandleLockstepHashPacket(State& state, const LockstepHashNetPacket& packet);
void RequestHostSnapshotResync(State& state, PlayerId target_peer_id);
void BeginJoinBarrierCatchup(State& state, PlayerId target_peer_id);
void BeginJoinBarrierTopologyChange(
    State& state,
    const NetTransportRuntime& transport,
    const std::vector<PlayerId>& joined_player_ids
);
void BeginJoinBarrierTopologyRemoval(
    State& state,
    const NetTransportRuntime& transport,
    const std::vector<PlayerId>& removed_player_ids
);
void HandleJoinBarrierStatus(State& state, const JoinBarrierStatusPacket& packet);
void HandleJoinBarrierResume(State& state, const JoinBarrierResumePacket& packet);
void HandleJoinBarrierTopology(
    State& state,
    Graphics& graphics,
    NetTransportRuntime& transport,
    const JoinBarrierTopologyPacket& packet
);
void HandleJoinBarrierTopologyAck(State& state, const JoinBarrierTopologyAckPacket& packet);
void SendPendingRunRestart(State& state, NetTransportRuntime& transport);
void HandleRunRestartPacket(State& state, const RunRestartPacket& packet);
bool ApplyDueRunRestart(State& state);
void NotifyRunRestartStageLoaded(State& state);
void HandleSnapshotResyncRequest(
    State& state,
    const Graphics& graphics,
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const SnapshotResyncRequestPacket& packet
);
void HandleSnapshotResyncChunk(
    State& state,
    Graphics& graphics,
    NetTransportRuntime& transport,
    const SnapshotResyncChunkPacket& packet
);
void HandleSnapshotResyncAck(State& state, const SnapshotResyncAckPacket& packet);
bool ScheduleLockstepSettingsChange(
    State& state,
    std::uint32_t input_delay_frames,
    std::uint32_t max_rollback_frames,
    std::string* status_out
);
void ApplyDueLockstepSettings(State& state);
void UpdateLockstepAutoDelay(State& state);
void RelayLockstepHashToOtherRemotes(
    NetTransportRuntime& transport,
    const NetEndpoint& source_endpoint,
    const LockstepHashNetPacket& packet
);
bool EnsureHostSyncedStage(State& state, std::string* status_out);
void MarkRemoteEndpointHeard(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint
);
void RemoveRemotePlayers(
    State& state,
    NetTransportRuntime& transport,
    const std::vector<PlayerId>& player_ids
);
void CleanupTimedOutRemoteEndpoints(State& state, NetTransportRuntime& transport);
void StepHostPackets(State& state, const Graphics& graphics, NetTransportRuntime& transport);
void StepPeerPackets(State& state, Graphics& graphics, NetTransportRuntime& transport);

void HandleJoinRequestAsHost(
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
void HandleLeaveNoticeAsHost(
    State& state,
    NetTransportRuntime& transport,
    const LeaveNoticePacket& leave
);

void EnsureSpawnedPlayer(
    State& state,
    PlayerId player_id,
    bool local,
    bool primary,
    sim::FxVec2 pos,
    const Graphics& graphics
);

} // namespace splonks::network

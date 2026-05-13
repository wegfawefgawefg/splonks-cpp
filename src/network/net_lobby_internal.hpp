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

Vec2 GetPrimaryPlayerSpawnPos(const State& state);
Vec2 GetRemoteSpawnPos(const State& state);
Vec2 GetEntranceOrRemoteSpawnPos(const State& state);
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
void FlushFuzzedOutgoingPackets(NetTransportRuntime& transport);

void SendJoinRequest(State& state);
void SendLeaveNotice(State& state);
bool IsInputLockstepSession(const State& state);
bool IsInputLockstepActive(const State& state);
void ResetInputLockstepState(State& state);
bool PrepareInputLockstepFrame(State& state, Graphics& graphics);
void HandleInputFrameRecords(State& state, const InputFrameRecordsPacket& packet);
void HandleLockstepSettingsPacket(State& state, const LockstepSettingsPacket& packet);
bool ScheduleLockstepSettingsChange(
    State& state,
    std::uint32_t input_delay_frames,
    std::uint32_t max_rollback_frames,
    std::string* status_out
);
void ApplyDueLockstepSettings(State& state);
void UpdateLockstepAutoDelay(State& state);
void RelayInputFrameRecordsToOtherRemotes(
    NetTransportRuntime& transport,
    const NetEndpoint& source_endpoint,
    const InputFrameRecordsPacket& packet
);
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
    const Vec2& pos,
    const Graphics& graphics
);

} // namespace splonks::network

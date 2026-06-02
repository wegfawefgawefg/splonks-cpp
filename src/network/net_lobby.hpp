#pragma once

#include "network/net_transport.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace splonks {

struct Audio;
struct Graphics;
struct State;

namespace network {

bool StartHostSession(State& state, std::uint16_t port, std::string* status_out);
bool StartHostSession(
    State& state,
    std::uint16_t port,
    std::uint32_t input_delay_frames,
    std::string* status_out
);
bool JoinHostSession(
    State& state,
    const std::string& host,
    std::uint16_t port,
    std::string* status_out
);
bool JoinHostSession(
    State& state,
    const std::string& host,
    std::uint16_t port,
    const std::vector<PlayerId>& preferred_player_ids,
    std::string* status_out
);
bool JoinHostSessionViaRealnetPunch(
    State& state,
    const NetEndpoint& punch_endpoint,
    const std::string& room_code,
    const std::string& join_attempt_id,
    const std::string& punch_secret,
    const std::vector<PlayerId>& preferred_player_ids,
    std::string* status_out
);
bool ConfigureHostRealnetPunch(
    State& state,
    const NetEndpoint& punch_endpoint,
    const std::string& room_code,
    const std::string& host_secret,
    std::string* status_out
);
bool JoinHostSessionViaRealnetRelay(
    State& state,
    const NetEndpoint& relay_endpoint,
    const std::string& room_code,
    const std::string& join_attempt_id,
    const std::string& relay_allocation_id,
    const std::string& relay_secret,
    const std::vector<PlayerId>& preferred_player_ids,
    std::string* status_out
);
bool ConfigureHostRealnetRelay(
    State& state,
    const NetEndpoint& relay_endpoint,
    const std::string& room_code,
    const std::string& host_secret,
    std::string* status_out
);
void DisconnectSession(State& state, std::string* status_out);
bool KickRemoteEndpoint(State& state, const std::string& address, std::uint16_t port,
                        std::string* status_out);
bool KickRemotePlayer(State& state, PlayerId player_id, std::string* status_out);
bool ReviveNetworkPlayersAtEntrance(State& state, const Graphics& graphics, std::string* status_out);
bool RespawnDeadNetworkPlayersAtEntrance(State& state, const Graphics& graphics, std::string* status_out);
bool RespawnLocalPlayersAtEntrance(State& state, const Graphics& graphics, std::string* status_out);
bool ReloadSyncedQuestStage(State& state, const Graphics& graphics, std::string* status_out);
bool RequestRunStart(State& state, std::uint32_t stage_seed, std::string* status_out);
bool RequestRunRestart(State& state, std::string* status_out);
bool ScheduleLockstepSettingsChange(
    State& state,
    std::uint32_t input_delay_frames,
    std::uint32_t max_rollback_frames,
    std::string* status_out
);
bool ForceLockstepSnapshotResync(State& state, PlayerId target_player_id, std::string* status_out);
bool IsInputLockstepSession(const State& state);
bool IsInputLockstepActive(const State& state);
bool IsInputLockstepCatchupBlocking(const State& state);
bool PrepareInputLockstepFrame(State& state, Graphics& graphics);
void MaintainInputLockstepTransport(State& state, Graphics& graphics);
void StepNetworkLobby(State& state, Graphics& graphics);
bool IsTransportOpen(const State& state);
std::uint16_t BoundTransportPort(const State& state);

} // namespace network
} // namespace splonks

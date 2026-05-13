#pragma once

#include "network/net_transport.hpp"
#include <cstdint>
#include <string>

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
bool JoinHostSession(State& state, const std::string& host, std::uint16_t port, std::string* status_out);
void DisconnectSession(State& state, std::string* status_out);
bool ReviveNetworkPlayersAtEntrance(State& state, const Graphics& graphics, std::string* status_out);
bool RespawnDeadNetworkPlayersAtEntrance(State& state, const Graphics& graphics, std::string* status_out);
bool RespawnLocalPlayersAtEntrance(State& state, const Graphics& graphics, std::string* status_out);
bool ReloadSyncedQuestStage(State& state, const Graphics& graphics, std::string* status_out);
bool ScheduleLockstepSettingsChange(
    State& state,
    std::uint32_t input_delay_frames,
    std::uint32_t max_rollback_frames,
    std::string* status_out
);
bool IsInputLockstepSession(const State& state);
bool IsInputLockstepActive(const State& state);
bool PrepareInputLockstepFrame(State& state, Graphics& graphics);
void StepNetworkLobby(State& state, Graphics& graphics);
bool IsTransportOpen(const State& state);
std::uint16_t BoundTransportPort(const State& state);

} // namespace network
} // namespace splonks

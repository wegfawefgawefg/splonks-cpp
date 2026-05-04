#pragma once

#include "network/net_transport.hpp"
#include <cstdint>
#include <string>

namespace splonks {

struct Graphics;
struct State;

namespace network {

bool StartHostSession(State& state, std::uint16_t port, std::string* status_out);
bool JoinHostSession(State& state, const std::string& host, std::uint16_t port, std::string* status_out);
void DisconnectSession(State& state, std::string* status_out);
bool RespawnLocalPlayersAtEntrance(State& state, const Graphics& graphics, std::string* status_out);
bool ReloadSyncedQuestStage(State& state, const Graphics& graphics, std::string* status_out);
void StepNetworkLobby(State& state, const Graphics& graphics);
bool IsTransportOpen(const State& state);
std::uint16_t BoundTransportPort(const State& state);

} // namespace network
} // namespace splonks

#include "network/net_lobby.hpp"

#include "network/net_ent_links.hpp"
#include "network/net_lobby_internal.hpp"
#include "network/net_protocol.hpp"
#include "state.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace splonks::network {

namespace {

constexpr PlayerId kFirstRemotePlayerId = 2;

NetTransportRuntime& EnsureTransport(State& state) {
    if (!state.net_transport) {
        state.net_transport = std::make_unique<NetTransportRuntime>(NetTransportRuntime::New());
    }
    return *state.net_transport;
}

} // namespace

bool StartHostSession(
    State& state,
    std::uint16_t port,
    std::uint32_t input_delay_frames,
    std::string* status_out
) {
    NetTransportRuntime& transport = EnsureTransport(state);
    std::string error;
    if (!transport.socket.Open(port, &error)) {
        if (status_out != nullptr) {
            *status_out = "Host failed: " + error;
        }
        return false;
    }

    state.net_session = NetSessionState::NewOffline();
    state.net_session.role = NetRole::Host;
    state.net_session.input_lockstep_enabled = true;
    state.net_session.lockstep_input_delay_frames = ClampLockstepInputDelayFrames(input_delay_frames);
    state.net_session.local_player_id = kPrimaryLocalPlayerId;
    state.net_session.host_player_id = kPrimaryLocalPlayerId;
    state.net_session.next_player_id = kFirstRemotePlayerId;
    if (!EnsureHostSyncedStage(state, status_out)) {
        transport.socket.Close();
        state.net_session = NetSessionState::NewOffline();
        return false;
    }
    state.net_session.stage_instance_id = static_cast<StageInstanceId>(state.net_session.stage_seed);
    state.players.EnsurePrimaryLocalPlayer();
    ResetInputLockstepState(state);
    RegisterStageEntLinks(state);
    transport.remotes.clear();
    transport.preferred_player_ids.clear();
    transport.join_request_pending = false;
    if (status_out != nullptr) {
        *status_out = "Hosting UDP on port " + std::to_string(transport.socket.BoundPort()) + ".";
        const std::vector<std::string> lan_addresses = GetLocalLanIpv4Addresses();
        if (!lan_addresses.empty()) {
            *status_out += " LAN join: " + lan_addresses.front() + ":" +
                std::to_string(transport.socket.BoundPort()) + ".";
        }
    }
    return true;
}

bool StartHostSession(State& state, std::uint16_t port, std::string* status_out) {
    return StartHostSession(state, port, kDefaultLockstepInputDelayFrames, status_out);
}

bool JoinHostSession(
    State& state,
    const std::string& host,
    std::uint16_t port,
    std::string* status_out
) {
    NetTransportRuntime& transport = EnsureTransport(state);
    std::string error;
    if (!transport.socket.Open(0, &error)) {
        if (status_out != nullptr) {
            *status_out = "Join failed: " + error;
        }
        return false;
    }

    state.net_session = NetSessionState::NewOffline();
    state.net_session.role = NetRole::Peer;
    state.net_session.input_lockstep_enabled = true;
    state.net_session.local_player_id = kPrimaryLocalPlayerId;
    state.net_session.host_player_id = kPrimaryLocalPlayerId;
    transport.host_endpoint = NetEndpoint{.address = host, .port = port};
    transport.remotes.clear();
    transport.join_request_pending = true;
    transport.join_request_retry_frames = 0;
    ResetInputLockstepState(state);
    SendJoinRequest(state);
    if (status_out != nullptr) {
        *status_out = "Joining " + EndpointToString(transport.host_endpoint) + ".";
    }
    return true;
}

void DisconnectSession(State& state, std::string* status_out) {
    if (state.net_transport) {
        SendLeaveNotice(state);
        state.net_transport->socket.Close();
        state.net_transport->remotes.clear();
        state.net_transport->join_request_pending = false;
    }
    state.net_session = NetSessionState::NewOffline();
    if (status_out != nullptr) {
        *status_out = "Disconnected.";
    }
}

} // namespace splonks::network

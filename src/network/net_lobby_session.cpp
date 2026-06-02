#include "network/net_ent_links.hpp"
#include "network/net_lobby.hpp"
#include "network/net_lobby_internal.hpp"
#include "network/net_protocol.hpp"
#include "state.hpp"

#include <algorithm>
#include <cstdint>
#include <gubsy/realnet/config.hpp>
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

PlayerId NextAvailablePlayerIdAfterExisting(const PlayerRegistry& players) {
    PlayerId next = kFirstRemotePlayerId;
    for (const PlayerSlot& slot : players.slots) {
        if (slot.player_id >= next) {
            next = slot.player_id + 1;
        }
    }
    return next;
}

} // namespace

bool StartHostSession(State& state, std::uint16_t port, std::uint32_t input_delay_frames,
                      std::string* status_out) {
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
    state.net_session.lockstep_input_delay_frames =
        ClampLockstepInputDelayFrames(input_delay_frames);
    state.net_session.local_player_id = kPrimaryLocalPlayerId;
    state.net_session.host_player_id = kPrimaryLocalPlayerId;
    if (!EnsureHostSyncedStage(state, status_out)) {
        transport.socket.Close();
        state.net_session = NetSessionState::NewOffline();
        return false;
    }
    state.net_session.stage_instance_id =
        static_cast<StageInstanceId>(state.net_session.stage_seed);
    state.players.EnsurePrimaryLocalPlayer();
    state.net_session.next_player_id = NextAvailablePlayerIdAfterExisting(state.players);
    ResetInputLockstepState(state);
    RegisterStageEntLinks(state);
    transport.remotes.clear();
    transport.pending_join_endpoints.clear();
    transport.preferred_player_ids.clear();
    transport.join_request_pending = false;
    transport.join_request_waiting_for_host = false;
    transport.join_pending_reason = JoinPendingReason::None;
    transport.realnet_punch = RealnetPunchRuntime{};
    transport.realnet_relay = RealnetRelayRuntime{};
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

bool JoinHostSession(State& state, const std::string& host, std::uint16_t port,
                     const std::vector<PlayerId>& preferred_player_ids, std::string* status_out) {
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
    transport.preferred_player_ids = preferred_player_ids;
    transport.host_endpoint = NetEndpoint{.address = host, .port = port};
    transport.remotes.clear();
    transport.pending_join_endpoints.clear();
    transport.join_request_pending = true;
    transport.join_request_waiting_for_host = false;
    transport.join_pending_reason = JoinPendingReason::None;
    transport.join_request_retry_frames = 0;
    transport.realnet_punch = RealnetPunchRuntime{};
    transport.realnet_relay = RealnetRelayRuntime{};
    ResetInputLockstepState(state);
    SendJoinRequest(state);
    if (status_out != nullptr) {
        *status_out = "Joining " + EndpointToString(transport.host_endpoint) + ".";
    }
    return true;
}

bool JoinHostSession(State& state, const std::string& host, std::uint16_t port,
                     std::string* status_out) {
    return JoinHostSession(state, host, port, {}, status_out);
}

bool JoinHostSessionViaRealnetPunch(
    State& state,
    const NetEndpoint& punch_endpoint,
    const std::string& room_code,
    const std::string& join_attempt_id,
    const std::string& punch_secret,
    const std::vector<PlayerId>& preferred_player_ids,
    std::string* status_out
) {
    NetTransportRuntime& transport = EnsureTransport(state);
    std::string error;
    if (!transport.socket.Open(0, &error)) {
        if (status_out != nullptr)
            *status_out = "Join failed: " + error;
        return false;
    }

    state.net_session = NetSessionState::NewOffline();
    state.net_session.role = NetRole::Peer;
    state.net_session.input_lockstep_enabled = true;
    state.net_session.local_player_id = kPrimaryLocalPlayerId;
    state.net_session.host_player_id = kPrimaryLocalPlayerId;
    transport.preferred_player_ids = preferred_player_ids;
    transport.host_endpoint = NetEndpoint{};
    transport.remotes.clear();
    transport.pending_join_endpoints.clear();
    transport.join_request_pending = true;
    transport.join_request_waiting_for_host = false;
    transport.join_pending_reason = JoinPendingReason::None;
    transport.join_request_retry_frames = 0;
    transport.realnet_punch = RealnetPunchRuntime{};
    transport.realnet_relay = RealnetRelayRuntime{};
    transport.realnet_punch.timing = realnet::default_config().punch;
    transport.realnet_punch.active = true;
    transport.realnet_punch.is_host = false;
    transport.realnet_punch.force = true;
    transport.realnet_punch.punch_endpoint = punch_endpoint;
    transport.realnet_punch.room_code = room_code;
    transport.realnet_punch.join_attempt_id = join_attempt_id;
    transport.realnet_punch.punch_secret = punch_secret;
    transport.realnet_punch.status = "starting_joiner_punch";
    ResetInputLockstepState(state);
    if (status_out != nullptr)
        *status_out = "Joining with Realnet NAT punch via " + EndpointToString(punch_endpoint) + ".";
    return true;
}

bool JoinHostSessionViaRealnetRelay(
    State& state,
    const NetEndpoint& relay_endpoint,
    const std::string& room_code,
    const std::string& join_attempt_id,
    const std::string& relay_allocation_id,
    const std::string& relay_secret,
    const std::vector<PlayerId>& preferred_player_ids,
    std::string* status_out
) {
    NetTransportRuntime& transport = EnsureTransport(state);
    std::string error;
    if (!transport.socket.Open(0, &error)) {
        if (status_out != nullptr)
            *status_out = "Join failed: " + error;
        return false;
    }

    state.net_session = NetSessionState::NewOffline();
    state.net_session.role = NetRole::Peer;
    state.net_session.input_lockstep_enabled = true;
    state.net_session.local_player_id = kPrimaryLocalPlayerId;
    state.net_session.host_player_id = kPrimaryLocalPlayerId;
    transport.preferred_player_ids = preferred_player_ids;
    transport.host_endpoint = relay_endpoint;
    transport.remotes.clear();
    transport.pending_join_endpoints.clear();
    transport.join_request_pending = true;
    transport.join_request_waiting_for_host = false;
    transport.join_pending_reason = JoinPendingReason::None;
    transport.join_request_retry_frames = 0;
    transport.realnet_punch = RealnetPunchRuntime{};
    transport.realnet_relay = RealnetRelayRuntime{};
    transport.realnet_relay.timing = realnet::default_config().relay.timing;
    transport.realnet_relay.active = true;
    transport.realnet_relay.is_host = false;
    transport.realnet_relay.relay_endpoint = relay_endpoint;
    transport.realnet_relay.room_code = room_code;
    transport.realnet_relay.join_attempt_id = join_attempt_id;
    transport.realnet_relay.relay_allocation_id = relay_allocation_id;
    transport.realnet_relay.relay_secret = relay_secret;
    transport.realnet_relay.status = "starting_joiner_relay";
    ResetInputLockstepState(state);
    if (status_out != nullptr)
        *status_out = "Joining with Realnet relay via " + EndpointToString(relay_endpoint) + ".";
    return true;
}

bool ConfigureHostRealnetPunch(
    State& state,
    const NetEndpoint& punch_endpoint,
    const std::string& room_code,
    const std::string& host_secret,
    std::string* status_out
) {
    if (!state.net_transport || !state.net_transport->socket.IsOpen()) {
        if (status_out != nullptr)
            *status_out = "Realnet punch host failed: transport is closed.";
        return false;
    }
    state.net_transport->realnet_punch = RealnetPunchRuntime{};
    state.net_transport->realnet_punch.timing = realnet::default_config().punch;
    state.net_transport->realnet_punch.active = true;
    state.net_transport->realnet_punch.is_host = true;
    state.net_transport->realnet_punch.punch_endpoint = punch_endpoint;
    state.net_transport->realnet_punch.room_code = room_code;
    state.net_transport->realnet_punch.host_secret = host_secret;
    state.net_transport->realnet_punch.status = "starting_host_punch";
    if (status_out != nullptr)
        *status_out = "Realnet punch host enabled via " + EndpointToString(punch_endpoint) + ".";
    return true;
}

bool ConfigureHostRealnetRelay(
    State& state,
    const NetEndpoint& relay_endpoint,
    const std::string& room_code,
    const std::string& host_secret,
    std::string* status_out
) {
    if (!state.net_transport || !state.net_transport->socket.IsOpen()) {
        if (status_out != nullptr)
            *status_out = "Realnet relay host failed: transport is closed.";
        return false;
    }
    state.net_transport->realnet_relay = RealnetRelayRuntime{};
    state.net_transport->realnet_relay.timing = realnet::default_config().relay.timing;
    state.net_transport->realnet_relay.active = true;
    state.net_transport->realnet_relay.is_host = true;
    state.net_transport->realnet_relay.relay_endpoint = relay_endpoint;
    state.net_transport->realnet_relay.room_code = room_code;
    state.net_transport->realnet_relay.host_secret = host_secret;
    state.net_transport->realnet_relay.status = "starting_host_relay";
    if (status_out != nullptr)
        *status_out = "Realnet relay host enabled via " + EndpointToString(relay_endpoint) + ".";
    return true;
}

void DisconnectSession(State& state, std::string* status_out) {
    if (state.net_transport) {
        SendLeaveNotice(state);
        state.net_transport->socket.Close();
        state.net_transport->remotes.clear();
        state.net_transport->pending_join_endpoints.clear();
        state.net_transport->join_request_pending = false;
        state.net_transport->join_request_waiting_for_host = false;
        state.net_transport->join_pending_reason = JoinPendingReason::None;
        state.net_transport->realnet_punch = RealnetPunchRuntime{};
        state.net_transport->realnet_relay = RealnetRelayRuntime{};
    }
    state.net_session = NetSessionState::NewOffline();
    if (status_out != nullptr) {
        *status_out = "Disconnected.";
    }
}

} // namespace splonks::network

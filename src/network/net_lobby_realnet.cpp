#include "network/net_lobby_internal.hpp"

#include "network/net_lobby.hpp"
#include "state.hpp"

#include <chrono>
#include <gubsy/realnet/config.hpp>
#include <gubsy/realnet/rendezvous.hpp>
#include <string>

namespace splonks::network {
namespace {

std::uint64_t NowMilliseconds() {
    using Clock = std::chrono::steady_clock;
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch())
            .count()
    );
}

NetEndpoint ToNetEndpoint(const realnet::Endpoint& endpoint) {
    return NetEndpoint{.address = endpoint.host, .port = endpoint.port};
}

bool SendRealnetPacket(NetTransportRuntime& transport, const NetEndpoint& endpoint,
                       realnet::Packet packet, const std::string& key) {
    packet.ts_ms = realnet::unix_time_ms();
    packet.seq = transport.realnet_punch.seq++;
    realnet::sign_packet(packet, key);
    const std::string encoded = realnet::encode_packet(packet);
    std::string error;
    if (!transport.socket.Send(endpoint,
                               reinterpret_cast<const std::uint8_t*>(encoded.data()),
                               encoded.size(),
                               &error)) {
        transport.last_error = error;
        return false;
    }
    return true;
}

const std::string& PacketKey(const RealnetPunchRuntime& punch, const realnet::Packet& packet) {
    if (packet.kind == realnet::PacketKind::EndpointHint && packet.role == "host")
        return punch.host_secret;
    if (punch.is_host)
        return punch.host_secret;
    return punch.punch_secret;
}

void SendHello(NetTransportRuntime& transport) {
    RealnetPunchRuntime& punch = transport.realnet_punch;
    realnet::Packet packet;
    packet.kind = punch.is_host ? realnet::PacketKind::HostHello
                                : realnet::PacketKind::JoinerHello;
    packet.room_code = punch.room_code;
    packet.join_attempt_id = punch.join_attempt_id;
    packet.role = punch.is_host ? "host" : "joiner";
    const std::string& key = punch.is_host ? punch.host_secret : punch.punch_secret;
    (void)SendRealnetPacket(transport, punch.punch_endpoint, packet, key);
    punch.hello_count += 1;
    punch.status = punch.is_host ? "sent_host_hello" : "sent_joiner_hello";
}

void SendProbe(NetTransportRuntime& transport) {
    RealnetPunchRuntime& punch = transport.realnet_punch;
    if (!punch.have_peer_endpoint)
        return;
    realnet::Packet packet;
    packet.kind = realnet::PacketKind::PunchProbe;
    packet.room_code = punch.room_code;
    packet.join_attempt_id = punch.join_attempt_id;
    packet.role = punch.is_host ? "host" : "joiner";
    (void)SendRealnetPacket(transport, punch.peer_endpoint, packet, punch.punch_secret);
    punch.probe_count += 1;
    punch.status = "sent_punch_probe";
}

void SendAck(NetTransportRuntime& transport, const NetEndpoint& endpoint) {
    RealnetPunchRuntime& punch = transport.realnet_punch;
    realnet::Packet packet;
    packet.kind = realnet::PacketKind::PunchAck;
    packet.room_code = punch.room_code;
    packet.join_attempt_id = punch.join_attempt_id;
    packet.role = punch.is_host ? "host" : "joiner";
    (void)SendRealnetPacket(transport, endpoint, packet, punch.punch_secret);
    punch.ack_count += 1;
    punch.status = "sent_punch_ack";
}

void SendPunchJoinRequest(State& state, NetTransportRuntime& transport) {
    RealnetPunchRuntime& punch = transport.realnet_punch;
    if (punch.is_host || !punch.have_peer_endpoint)
        return;
    transport.host_endpoint = punch.peer_endpoint;
    SendJoinRequest(state);
    punch.sent_join_request = true;
    punch.join_request_count += 1;
    punch.status = "join_request_sent";
}

bool PacketLooksLikeJson(const UdpPacket& packet) {
    return packet.size > 0 && packet.bytes[0] == static_cast<std::uint8_t>('{');
}

} // namespace

void MaintainRealnetPunch(State& state, NetTransportRuntime& transport) {
    RealnetPunchRuntime& punch = transport.realnet_punch;
    if (!punch.active)
        return;

    const std::uint64_t now_ms = NowMilliseconds();
    if (punch.deadline_ms == 0)
        punch.deadline_ms = now_ms + punch.timing.punch_window_ms;
    if (punch.next_hello_ms <= now_ms) {
        SendHello(transport);
        punch.next_hello_ms = now_ms + punch.timing.hello_interval_ms;
    }
    if (punch.have_peer_endpoint && punch.next_probe_ms <= now_ms && now_ms <= punch.deadline_ms) {
        SendProbe(transport);
        SendPunchJoinRequest(state, transport);
        punch.next_probe_ms = now_ms + punch.timing.probe_interval_ms;
    }
    if (!punch.timed_out && now_ms > punch.deadline_ms && !punch.established) {
        punch.timed_out = true;
        punch.status = "failed";
        punch.failure_reason = punch.have_peer_endpoint
                                   ? "punch_probe_timeout"
                                   : "endpoint_hint_timeout";
    }
}

bool TryHandleRealnetPunchPacket(State& state, NetTransportRuntime& transport,
                                 const UdpPacket& packet) {
    RealnetPunchRuntime& punch = transport.realnet_punch;
    if (!punch.active || !PacketLooksLikeJson(packet))
        return false;

    realnet::Packet decoded;
    std::string err;
    const std::string bytes(reinterpret_cast<const char*>(packet.bytes.data()), packet.size);
    if (!realnet::decode_packet(bytes, decoded, err))
        return false;
    if (decoded.room_code != punch.room_code)
        return true;

    const std::string& key = PacketKey(punch, decoded);
    if (key.empty() || !realnet::verify_packet(decoded, key))
        return true;

    if (decoded.kind == realnet::PacketKind::EndpointHint && decoded.peer_endpoint.has_value()) {
        if (punch.is_host && !decoded.punch_secret.empty())
            punch.punch_secret = decoded.punch_secret;
        punch.peer_endpoint = ToNetEndpoint(*decoded.peer_endpoint);
        punch.have_peer_endpoint = punch.peer_endpoint.port != 0;
        punch.deadline_ms = NowMilliseconds() + punch.timing.punch_window_ms;
        punch.hint_count += 1;
        punch.status = "endpoint_hint_received";
        punch.failure_reason.clear();
        punch.timed_out = false;
        if (!punch.is_host && punch.have_peer_endpoint) {
            SendPunchJoinRequest(state, transport);
        }
        return true;
    }

    if (decoded.kind == realnet::PacketKind::PunchProbe) {
        if (!punch.have_peer_endpoint) {
            punch.peer_endpoint = packet.endpoint;
            punch.have_peer_endpoint = true;
        }
        punch.status = "punch_probe_received";
        punch.established = true;
        punch.failure_reason.clear();
        punch.timed_out = false;
        SendAck(transport, packet.endpoint);
        return true;
    }

    if (decoded.kind == realnet::PacketKind::PunchAck) {
        if (!punch.have_peer_endpoint) {
            punch.peer_endpoint = packet.endpoint;
            punch.have_peer_endpoint = true;
        }
        if (!punch.is_host) {
            punch.peer_endpoint = packet.endpoint;
            punch.have_peer_endpoint = true;
            SendPunchJoinRequest(state, transport);
            punch.established = true;
        }
        punch.status = "punch_ack_received";
        punch.established = true;
        punch.failure_reason.clear();
        punch.timed_out = false;
        return true;
    }

    return true;
}

} // namespace splonks::network

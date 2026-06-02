#include "network/net_lobby_internal.hpp"

#include "network/net_lobby.hpp"
#include "state.hpp"

#include <chrono>
#include <algorithm>
#include <gubsy/realnet/config.hpp>
#include <gubsy/realnet/rendezvous.hpp>
#include <gubsy/realnet/relay.hpp>
#include <string>
#include <vector>

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

bool SendRelayPacket(NetTransportRuntime& transport, realnet::RelayPacket packet,
                     const std::string& key) {
    RealnetRelayRuntime& relay = transport.realnet_relay;
    packet.ts_ms = realnet::unix_time_ms();
    packet.seq = relay.seq++;
    realnet::sign_relay_packet(packet, key);
    const std::string encoded = realnet::encode_relay_packet(packet);
    std::string error;
    if (!transport.socket.Send(relay.relay_endpoint,
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

const std::string& RelayPacketKey(const RealnetRelayRuntime& relay) {
    return relay.is_host ? relay.host_secret : relay.relay_secret;
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

void SendRelayHello(NetTransportRuntime& transport) {
    RealnetRelayRuntime& relay = transport.realnet_relay;
    realnet::RelayPacket packet;
    packet.kind = realnet::RelayPacketKind::Hello;
    packet.role = relay.is_host ? realnet::RelayRole::Host : realnet::RelayRole::Joiner;
    packet.room_code = relay.room_code;
    packet.join_attempt_id = relay.join_attempt_id;
    packet.allocation_id = relay.relay_allocation_id;
    const std::string& key = RelayPacketKey(relay);
    (void)SendRelayPacket(transport, packet, key);
    relay.hello_count += 1;
    relay.status = relay.is_host ? "sent_host_relay_hello" : "sent_joiner_relay_hello";
}

void SendRelayKeepalive(NetTransportRuntime& transport) {
    RealnetRelayRuntime& relay = transport.realnet_relay;
    if (!relay.ready)
        return;
    realnet::RelayPacket packet;
    packet.kind = realnet::RelayPacketKind::Keepalive;
    packet.role = relay.is_host ? realnet::RelayRole::Host : realnet::RelayRole::Joiner;
    packet.room_code = relay.room_code;
    packet.join_attempt_id = relay.join_attempt_id;
    packet.allocation_id = relay.relay_allocation_id;
    const std::string& key = RelayPacketKey(relay);
    (void)SendRelayPacket(transport, packet, key);
    relay.keepalive_count += 1;
    relay.status = "sent_relay_keepalive";
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

bool PacketLooksLikeRelay(const UdpPacket& packet) {
    return packet.size >= 4 &&
           packet.bytes[0] == static_cast<std::uint8_t>('G') &&
           packet.bytes[1] == static_cast<std::uint8_t>('R') &&
           packet.bytes[2] == static_cast<std::uint8_t>('L') &&
           packet.bytes[3] == static_cast<std::uint8_t>('Y');
}

NetEndpoint RelayEndpointForAllocation(RealnetRelayRuntime& relay,
                                       const std::string& allocation_id) {
    for (const RealnetRelayRoute& route : relay.routes) {
        if (route.allocation_id == allocation_id)
            return route.endpoint;
    }

    const NetEndpoint endpoint{.address = "realnet-relay",
                               .port = relay.next_virtual_port++,
                               .kind = NetEndpointKind::RealnetRelayVirtual};
    relay.routes.push_back(RealnetRelayRoute{.allocation_id = allocation_id,
                                             .endpoint = endpoint});
    return endpoint;
}

std::string RelayAllocationForEndpoint(const RealnetRelayRuntime& relay,
                                       const NetEndpoint& endpoint) {
    if (!relay.is_host)
        return relay.relay_allocation_id;
    for (const RealnetRelayRoute& route : relay.routes) {
        if (EndpointsEqual(route.endpoint, endpoint))
            return route.allocation_id;
    }
    return {};
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

void MaintainRealnetRelay(State& state, NetTransportRuntime& transport) {
    RealnetRelayRuntime& relay = transport.realnet_relay;
    if (!relay.active)
        return;

    const std::uint64_t now_ms = NowMilliseconds();
    if (relay.next_hello_ms <= now_ms && !relay.ready) {
        SendRelayHello(transport);
        relay.next_hello_ms = now_ms + relay.timing.hello_interval_ms;
    }
    if (relay.next_keepalive_ms <= now_ms && relay.ready) {
        SendRelayKeepalive(transport);
        relay.next_keepalive_ms = now_ms + relay.timing.keepalive_interval_ms;
    }
    if (!relay.is_host && relay.ready && transport.join_request_pending &&
        transport.join_request_retry_frames == 0) {
        transport.host_endpoint = relay.relay_endpoint;
        SendJoinRequest(state);
        relay.status = "relay_join_request_sent";
    }
}

bool WrapRealnetRelayPacket(NetTransportRuntime& transport,
                            const UdpPacket& packet,
                            UdpPacket& out) {
    RealnetRelayRuntime& relay = transport.realnet_relay;
    if (!relay.active)
        return false;
    const bool to_relay_server = EndpointsEqual(packet.endpoint, relay.relay_endpoint);
    const std::string allocation_id = RelayAllocationForEndpoint(relay, packet.endpoint);
    if (!to_relay_server && allocation_id.empty())
        return false;
    if (!relay.ready) {
        relay.status = "relay_waiting_ready";
        out = UdpPacket{};
        return true;
    }

    realnet::RelayPacket relay_packet;
    relay_packet.kind = realnet::RelayPacketKind::Data;
    relay_packet.role = relay.is_host ? realnet::RelayRole::Host : realnet::RelayRole::Joiner;
    relay_packet.room_code = relay.room_code;
    relay_packet.join_attempt_id = relay.join_attempt_id;
    relay_packet.allocation_id = allocation_id.empty() ? relay.relay_allocation_id : allocation_id;
    if (relay_packet.allocation_id.empty()) {
        relay.failure_reason = "relay_allocation_missing";
        out = UdpPacket{};
        return true;
    }
    relay_packet.payload.assign(packet.bytes.begin(),
                                packet.bytes.begin() + static_cast<std::ptrdiff_t>(packet.size));
    const std::string& key = RelayPacketKey(relay);
    realnet::sign_relay_packet(relay_packet, key);
    const std::string encoded = realnet::encode_relay_packet(relay_packet);
    if (encoded.empty() || encoded.size() > out.bytes.size()) {
        relay.failure_reason = "relay_packet_too_large";
        out = UdpPacket{};
        return true;
    }

    out.endpoint = relay.relay_endpoint;
    out.size = encoded.size();
    std::copy(encoded.begin(), encoded.end(), out.bytes.begin());
    relay.data_sent_count += 1;
    return true;
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

bool TryHandleRealnetRelayPacket(State& state,
                                 NetTransportRuntime& transport,
                                 const UdpPacket& packet,
                                 UdpPacket& unwrapped) {
    RealnetRelayRuntime& relay = transport.realnet_relay;
    if (!relay.active || !EndpointsEqual(packet.endpoint, relay.relay_endpoint) ||
        !PacketLooksLikeRelay(packet)) {
        return false;
    }

    realnet::RelayPacket decoded;
    std::string err;
    const std::string bytes(reinterpret_cast<const char*>(packet.bytes.data()), packet.size);
    if (!realnet::decode_relay_packet(bytes, decoded, err)) {
        relay.failure_reason = err;
        return true;
    }
    if (decoded.room_code != relay.room_code)
        return true;

    const std::string& key = RelayPacketKey(relay);
    if (key.empty() || !realnet::verify_relay_packet(decoded, key)) {
        relay.failure_reason = "relay_mac";
        return true;
    }

    if (decoded.kind == realnet::RelayPacketKind::Ready) {
        if (!decoded.allocation_id.empty()) {
            relay.relay_allocation_id = decoded.allocation_id;
            if (relay.is_host)
                (void)RelayEndpointForAllocation(relay, decoded.allocation_id);
        }
        relay.ready = true;
        relay.ready_count += 1;
        relay.status = "relay_ready";
        relay.failure_reason.clear();
        if (!relay.is_host) {
            transport.host_endpoint = relay.relay_endpoint;
            SendJoinRequest(state);
        }
        unwrapped = UdpPacket{};
        return true;
    }

    if (decoded.kind == realnet::RelayPacketKind::Data) {
        if (decoded.payload.size() > kNetPacketMaxBytes) {
            relay.failure_reason = "relay_payload_too_large";
            return true;
        }
        unwrapped.endpoint = relay.is_host
            ? RelayEndpointForAllocation(relay, decoded.allocation_id)
            : relay.relay_endpoint;
        unwrapped.size = decoded.payload.size();
        std::copy(decoded.payload.begin(), decoded.payload.end(), unwrapped.bytes.begin());
        relay.data_received_count += 1;
        relay.status = "relay_data_received";
        relay.failure_reason.clear();
        return true;
    }

    unwrapped = UdpPacket{};
    return true;
}

} // namespace splonks::network

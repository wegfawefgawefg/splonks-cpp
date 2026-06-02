#pragma once

#include "network/net_fuzzer.hpp"
#include "network/net_limits.hpp"
#include "network/net_protocol.hpp"
#include "player_id.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <gubsy/realnet/config.hpp>

namespace splonks::network {

enum class NetEndpointKind {
    Udp,
    RealnetRelayVirtual,
};

struct NetEndpoint {
    std::string address = "127.0.0.1";
    std::uint16_t port = 0;
    NetEndpointKind kind = NetEndpointKind::Udp;
};

struct UdpPacket {
    NetEndpoint endpoint;
    std::array<std::uint8_t, kNetTransportDatagramMaxBytes> bytes{};
    std::size_t size = 0;
};

struct NetFuzzedOutgoingPacket {
    UdpPacket packet;
    std::uint64_t due_time_ms = 0;
    std::uint64_t sequence = 0;
};

class UdpSocket {
public:
    UdpSocket() = default;
    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;
    UdpSocket(UdpSocket&& other) noexcept;
    UdpSocket& operator=(UdpSocket&& other) noexcept;
    ~UdpSocket();

    bool Open(std::uint16_t bind_port, std::string* error_out);
    void Close();
    bool IsOpen() const;
    std::uint16_t BoundPort() const;

    bool Send(const NetEndpoint& endpoint, const std::uint8_t* bytes, std::size_t size, std::string* error_out);
    std::optional<UdpPacket> Receive(std::string* error_out);

private:
#ifdef _WIN32
    std::uintptr_t fd_ = ~std::uintptr_t{0};
#else
    int fd_ = -1;
#endif
    std::uint16_t bound_port_ = 0;
};

struct NetRemoteEndpoint {
    std::vector<PlayerId> player_ids;
    NetEndpoint endpoint;
    std::uint64_t last_heard_frame = 0;
    std::uint64_t last_heard_pump_tick = 0;
    std::uint64_t next_ping_send_time_ms = 0;
    std::uint32_t next_ping_sequence = 1;
};

struct NetPendingJoinEndpoint {
    NetEndpoint endpoint;
    JoinRequestPacket request;
    std::uint64_t last_heard_pump_tick = 0;
};

struct RealnetPunchRuntime {
    bool active = false;
    bool is_host = false;
    bool force = false;
    NetEndpoint punch_endpoint;
    NetEndpoint peer_endpoint;
    bool have_peer_endpoint = false;
    std::string room_code;
    std::string host_secret;
    std::string join_attempt_id;
    std::string punch_secret;
    std::uint64_t deadline_ms = 0;
    std::uint64_t next_hello_ms = 0;
    std::uint64_t next_probe_ms = 0;
    std::uint64_t seq = 1;
    realnet::PunchTimingConfig timing;
    bool timed_out = false;
    bool established = false;
    bool sent_join_request = false;
    std::string status{"idle"};
    std::string failure_reason;
    std::uint64_t hello_count = 0;
    std::uint64_t probe_count = 0;
    std::uint64_t ack_count = 0;
    std::uint64_t hint_count = 0;
    std::uint64_t join_request_count = 0;
};

struct RealnetRelayRoute {
    std::string allocation_id;
    NetEndpoint endpoint;
};

struct RealnetRelayRuntime {
    bool active = false;
    bool is_host = false;
    bool ready = false;
    NetEndpoint relay_endpoint;
    std::string room_code;
    std::string host_secret;
    std::string join_attempt_id;
    std::string relay_allocation_id;
    std::string relay_secret;
    std::uint64_t next_hello_ms = 0;
    std::uint64_t next_keepalive_ms = 0;
    std::uint64_t seq = 1;
    realnet::RelayTimingConfig timing;
    std::string status{"idle"};
    std::string failure_reason;
    std::uint64_t hello_count = 0;
    std::uint64_t ready_count = 0;
    std::uint64_t keepalive_count = 0;
    std::uint64_t data_sent_count = 0;
    std::uint64_t data_received_count = 0;
    std::uint64_t remote_timeout_pump_ticks = 3600;
    std::uint16_t next_virtual_port = 60000;
    std::vector<RealnetRelayRoute> routes;
};

struct NetTransportRuntime {
    UdpSocket socket;
    std::vector<NetRemoteEndpoint> remotes;
    std::vector<NetPendingJoinEndpoint> pending_join_endpoints;
    std::vector<PlayerId> preferred_player_ids;
    NetEndpoint host_endpoint;
    bool join_request_pending = false;
    bool join_request_waiting_for_host = false;
    JoinPendingReason join_pending_reason = JoinPendingReason::None;
    std::uint32_t join_request_retry_frames = 0;
    std::uint64_t next_host_ping_send_time_ms = 0;
    std::uint32_t next_host_ping_sequence = 1;
    NetFuzzerConfig fuzzer_config;
    NetFuzzerStats fuzzer_stats;
    std::vector<NetFuzzedOutgoingPacket> fuzzed_outgoing_packets;
    std::uint64_t next_fuzzed_packet_sequence = 1;
    std::uint64_t fuzzer_next_bandwidth_send_time_ms = 0;
    std::uint32_t fuzzer_rng_state = 0xA341316CU;
    std::uint32_t fuzzer_burst_packets_remaining = 0;
    std::uint64_t pump_tick = 0;
    std::string last_error;
    RealnetPunchRuntime realnet_punch;
    RealnetRelayRuntime realnet_relay;
    bool capture_outgoing_packets = false;
    std::vector<UdpPacket> captured_packets;

    static NetTransportRuntime New();
};

bool EndpointsEqual(const NetEndpoint& a, const NetEndpoint& b);
bool IsRealnetRelayVirtualEndpoint(const NetEndpoint& endpoint);
std::string EndpointToString(const NetEndpoint& endpoint);
std::vector<std::string> GetLocalLanIpv4Addresses();

} // namespace splonks::network

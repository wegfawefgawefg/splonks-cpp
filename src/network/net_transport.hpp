#pragma once

#include "network/net_ids.hpp"
#include "frame_data_id.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace splonks::network {

struct NetEndpoint {
    std::string address = "127.0.0.1";
    std::uint16_t port = 0;
};

struct UdpPacket {
    NetEndpoint endpoint;
    std::array<std::uint8_t, 512> bytes{};
    std::size_t size = 0;
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
    int fd_ = -1;
    std::uint16_t bound_port_ = 0;
};

struct NetRemoteEndpoint {
    std::vector<PlayerId> player_ids;
    NetEndpoint endpoint;
    std::uint64_t last_heard_frame = 0;
    std::uint64_t highest_acked_coordinator_order = 0;
};

struct NetRemotePlayerTarget {
    PlayerId player_id = kInvalidPlayerId;
    float start_pos_x = 0.0F;
    float start_pos_y = 0.0F;
    float pos_x = 0.0F;
    float pos_y = 0.0F;
    float vel_x = 0.0F;
    float vel_y = 0.0F;
    std::uint8_t facing = 0;
    std::uint8_t condition = 0;
    std::uint8_t grounded = 0;
    std::uint8_t animate = 0;
    FrameDataId animation_id = kInvalidFrameDataId;
    std::uint16_t animation_frame = 0;
    std::uint16_t animation_flags = 0;
    float animation_time = 0.0F;
    float animation_speed = 1.0F;
    std::uint32_t sequence = 0;
    std::uint64_t interpolation_start_frame = 0;
    std::uint64_t last_received_frame = 0;
};

struct NetTransportRuntime {
    UdpSocket socket;
    std::vector<NetRemoteEndpoint> remotes;
    std::vector<NetRemotePlayerTarget> remote_player_targets;
    NetEndpoint coordinator_endpoint;
    bool join_request_pending = false;
    std::uint32_t join_request_retry_frames = 0;
    std::uint32_t next_snapshot_sequence = 1;
    std::uint32_t snapshot_send_interval_frames = 3;
    float remote_interpolation_strength = 0.35F;
    std::uint32_t remote_interpolation_delay_frames = 3;
    float remote_snap_distance = 32.0F;
    std::string last_error;

    static NetTransportRuntime New();
};

bool EndpointsEqual(const NetEndpoint& a, const NetEndpoint& b);
std::string EndpointToString(const NetEndpoint& endpoint);

} // namespace splonks::network

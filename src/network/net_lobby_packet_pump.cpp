#include "network/net_lobby_internal.hpp"

#include "graphics.hpp"
#include "network/net_progression.hpp"
#include "network/net_protocol.hpp"
#include "network/net_transport.hpp"
#include "state.hpp"

#include <cstdint>
#include <chrono>
#include <cmath>
#include <optional>
#include <string>

namespace splonks::network {

namespace {

constexpr std::uint32_t kJoinRetryFrames = 30;
constexpr std::uint64_t kPingIntervalMs = 500;

std::uint64_t NowMilliseconds() {
    using Clock = std::chrono::steady_clock;
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now().time_since_epoch()
        ).count()
    );
}

void UpdatePeerPing(State& state, const NetEndpoint& endpoint, float rtt_ms) {
    for (NetPeerState& peer : state.net_session.peers) {
        if (peer.endpoint_address != endpoint.address || peer.endpoint_port != endpoint.port) {
            continue;
        }

        const float previous = peer.estimated_ping_ms;
        peer.estimated_ping_ms = previous <= 0.0F
            ? rtt_ms
            : (previous * 0.8F) + (rtt_ms * 0.2F);

        const float sample_jitter = previous <= 0.0F ? 0.0F : std::fabs(rtt_ms - previous);
        peer.jitter_ms = (peer.jitter_ms * 0.8F) + (sample_jitter * 0.2F);
        return;
    }
}

void SendPingPacket(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    PlayerId local_player_id,
    std::uint32_t sequence,
    std::uint64_t now_ms
) {
    PingPacket ping;
    ping.sender_peer_id = local_player_id;
    ping.sequence = sequence;
    ping.sent_time_ms = now_ms;
    SendEncodedPacket(transport, endpoint, EncodePing(ping));
}

void SendDueHostPings(State& state, NetTransportRuntime& transport) {
    const std::uint64_t now_ms = NowMilliseconds();
    for (NetRemoteEndpoint& remote : transport.remotes) {
        if (remote.next_ping_send_time_ms > now_ms) {
            continue;
        }
        SendPingPacket(
            transport,
            remote.endpoint,
            state.net_session.local_player_id,
            remote.next_ping_sequence++,
            now_ms
        );
        remote.next_ping_send_time_ms = now_ms + kPingIntervalMs;
    }
}

void SendDuePeerPing(State& state, NetTransportRuntime& transport) {
    if (transport.join_request_pending) {
        return;
    }
    const std::uint64_t now_ms = NowMilliseconds();
    if (transport.next_host_ping_send_time_ms > now_ms) {
        return;
    }
    SendPingPacket(
        transport,
        transport.host_endpoint,
        state.net_session.local_player_id,
        transport.next_host_ping_sequence++,
        now_ms
    );
    transport.next_host_ping_send_time_ms = now_ms + kPingIntervalMs;
}

void ReplyToPing(
    State& state,
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const PingPacket& ping
) {
    PongPacket pong;
    pong.sender_peer_id = state.net_session.local_player_id;
    pong.sequence = ping.sequence;
    pong.echoed_sent_time_ms = ping.sent_time_ms;
    SendEncodedPacket(transport, endpoint, EncodePong(pong));
}

void HandlePong(State& state, const NetEndpoint& endpoint, const PongPacket& pong) {
    const std::uint64_t now_ms = NowMilliseconds();
    if (pong.echoed_sent_time_ms > now_ms) {
        return;
    }
    UpdatePeerPing(state, endpoint, static_cast<float>(now_ms - pong.echoed_sent_time_ms));
}

} // namespace

void StepHostPackets(State& state, const Graphics& graphics, NetTransportRuntime& transport) {
    SendDueHostPings(state, transport);
    for (int i = 0; i < 64; ++i) {
        std::string error;
        const std::optional<UdpPacket> packet = transport.socket.Receive(&error);
        if (!error.empty()) {
            transport.last_error = error;
        }
        if (!packet.has_value()) {
            CleanupTimedOutRemoteEndpoints(state, transport);
            return;
        }
        transport.fuzzer_stats.packets_received += 1U;

        MarkRemoteEndpointHeard(transport, packet->endpoint, state.frame);

        if (const std::optional<JoinRequestPacket> request =
                TryDecodeJoinRequest(packet->bytes.data(), packet->size)) {
            HandleJoinRequestAsHost(state, graphics, transport, *packet, *request);
            continue;
        }

        if (const std::optional<LeaveNoticePacket> leave =
                TryDecodeLeaveNotice(packet->bytes.data(), packet->size)) {
            HandleLeaveNoticeAsHost(state, transport, *leave);
            continue;
        }

        if (const std::optional<PingPacket> ping =
                TryDecodePing(packet->bytes.data(), packet->size)) {
            ReplyToPing(state, transport, packet->endpoint, *ping);
            continue;
        }

        if (const std::optional<PongPacket> pong =
                TryDecodePong(packet->bytes.data(), packet->size)) {
            HandlePong(state, packet->endpoint, *pong);
            continue;
        }

        if (const std::optional<InputFrameRecordsPacket> input_frames =
                TryDecodeInputFrameRecords(packet->bytes.data(), packet->size)) {
            HandleInputFrameRecords(state, *input_frames);
            RelayInputFrameRecordsToOtherRemotes(transport, packet->endpoint, *input_frames);
            continue;
        }

        if (const std::optional<LockstepSettingsPacket> settings =
                TryDecodeLockstepSettings(packet->bytes.data(), packet->size)) {
            // Settings are host-owned. Ignore peer-originated settings packets.
            continue;
        }

        if (const std::optional<LockstepHashNetPacket> hash =
                TryDecodeLockstepHash(packet->bytes.data(), packet->size)) {
            HandleLockstepHashPacket(state, *hash);
            RelayLockstepHashToOtherRemotes(transport, packet->endpoint, *hash);
            continue;
        }

        if (const std::optional<SnapshotResyncRequestPacket> request =
                TryDecodeSnapshotResyncRequest(packet->bytes.data(), packet->size)) {
            HandleSnapshotResyncRequest(state, graphics, transport, packet->endpoint, *request);
            continue;
        }

        if (const std::optional<SnapshotResyncAckPacket> ack =
                TryDecodeSnapshotResyncAck(packet->bytes.data(), packet->size)) {
            HandleSnapshotResyncAck(state, *ack);
            continue;
        }
    }
}

void StepPeerPackets(State& state, Graphics& graphics, NetTransportRuntime& transport) {
    if (transport.join_request_pending) {
        if (transport.join_request_retry_frames == 0) {
            SendJoinRequest(state);
            transport.join_request_retry_frames = kJoinRetryFrames;
        } else {
            transport.join_request_retry_frames -= 1;
        }
    }
    SendDuePeerPing(state, transport);

    for (int i = 0; i < 64; ++i) {
        std::string error;
        const std::optional<UdpPacket> packet = transport.socket.Receive(&error);
        if (!error.empty()) {
            transport.last_error = error;
        }
        if (!packet.has_value()) {
            return;
        }
        transport.fuzzer_stats.packets_received += 1U;

        if (const std::optional<LeaveNoticePacket> leave =
                TryDecodeLeaveNotice(packet->bytes.data(), packet->size)) {
            HandleLeaveNoticeAsHost(state, transport, *leave);
            continue;
        }

        if (const std::optional<PingPacket> ping =
                TryDecodePing(packet->bytes.data(), packet->size)) {
            ReplyToPing(state, transport, packet->endpoint, *ping);
            continue;
        }

        if (const std::optional<PongPacket> pong =
                TryDecodePong(packet->bytes.data(), packet->size)) {
            HandlePong(state, packet->endpoint, *pong);
            continue;
        }

        if (const std::optional<JoinAcceptPacket> accept =
                TryDecodeJoinAccept(packet->bytes.data(), packet->size)) {
            HandleJoinAcceptAsPeer(state, graphics, transport, *accept);
            continue;
        }

        if (const std::optional<InputFrameRecordsPacket> input_frames =
                TryDecodeInputFrameRecords(packet->bytes.data(), packet->size)) {
            HandleInputFrameRecords(state, *input_frames);
            continue;
        }

        if (const std::optional<LockstepSettingsPacket> settings =
                TryDecodeLockstepSettings(packet->bytes.data(), packet->size)) {
            HandleLockstepSettingsPacket(state, *settings);
            continue;
        }

        if (const std::optional<LockstepHashNetPacket> hash =
                TryDecodeLockstepHash(packet->bytes.data(), packet->size)) {
            HandleLockstepHashPacket(state, *hash);
            continue;
        }

        if (const std::optional<SnapshotResyncChunkPacket> chunk =
                TryDecodeSnapshotResyncChunk(packet->bytes.data(), packet->size)) {
            HandleSnapshotResyncChunk(state, graphics, transport, *chunk);
            continue;
        }
    }
}

} // namespace splonks::network

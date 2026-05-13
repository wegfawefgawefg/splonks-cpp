#include "network/net_lobby_internal.hpp"

#include "graphics.hpp"
#include "network/net_progression.hpp"
#include "network/net_protocol.hpp"
#include "network/net_transport.hpp"
#include "state.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace splonks::network {

namespace {

constexpr std::uint32_t kJoinRetryFrames = 30;

} // namespace

void StepHostPackets(State& state, const Graphics& graphics, NetTransportRuntime& transport) {
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

        if (const std::optional<InputFrameRecordsPacket> input_frames =
                TryDecodeInputFrameRecords(packet->bytes.data(), packet->size)) {
            HandleInputFrameRecords(state, *input_frames);
            RelayInputFrameRecordsToOtherRemotes(transport, packet->endpoint, *input_frames);
            continue;
        }
    }
}

void StepPeerPackets(State& state, const Graphics& graphics, NetTransportRuntime& transport) {
    if (transport.join_request_pending) {
        if (transport.join_request_retry_frames == 0) {
            SendJoinRequest(state);
            transport.join_request_retry_frames = kJoinRetryFrames;
        } else {
            transport.join_request_retry_frames -= 1;
        }
    }

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
    }
}

} // namespace splonks::network

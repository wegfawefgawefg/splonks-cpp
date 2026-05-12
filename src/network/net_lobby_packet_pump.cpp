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
            HandleJoinRequestAsCoordinator(state, graphics, transport, *packet, *request);
            continue;
        }

        if (const std::optional<LeaveNoticePacket> leave =
                TryDecodeLeaveNotice(packet->bytes.data(), packet->size)) {
            HandleLeaveNoticeAsCoordinator(state, transport, *leave);
            continue;
        }

        if (const std::optional<DurableMessageAckPacket> ack =
                TryDecodeDurableMessageAck(packet->bytes.data(), packet->size)) {
            HandleDurableMessageAckAsCoordinator(state, transport, packet->endpoint, *ack);
            continue;
        }

        if (const std::optional<PlayerSnapshotsPacket> snapshots =
                TryDecodePlayerSnapshots(packet->bytes.data(), packet->size)) {
            ApplyPlayerSnapshots(state, graphics, transport, *snapshots);
            RelaySnapshotsToOtherRemotes(transport, packet->endpoint, *snapshots);
            continue;
        }

        if (const std::optional<ActionRequestMessagesPacket> action_requests =
                TryDecodeActionRequestMessages(packet->bytes.data(), packet->size)) {
            HandleActionRequestMessagesAsCoordinator(state, transport, packet->endpoint, *action_requests);
            continue;
        }

        if (const std::optional<PresentationCommandMessagesPacket> presentation_messages =
                TryDecodePresentationCommandMessages(packet->bytes.data(), packet->size)) {
            HandlePresentationCommandMessagesAsCoordinator(state, *presentation_messages);
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
            HandleLeaveNoticeAsCoordinator(state, transport, *leave);
            continue;
        }

        if (const std::optional<JoinAcceptPacket> accept =
                TryDecodeJoinAccept(packet->bytes.data(), packet->size)) {
            HandleJoinAcceptAsPeer(state, graphics, transport, *accept);
            continue;
        }

        if (const std::optional<StageSyncPacket> stage_sync =
                TryDecodeStageSync(packet->bytes.data(), packet->size)) {
            ApplyStageSync(state, graphics, transport, *stage_sync);
            continue;
        }

        if (const std::optional<PlayerSnapshotsPacket> snapshots =
                TryDecodePlayerSnapshots(packet->bytes.data(), packet->size)) {
            ApplyPlayerSnapshots(state, graphics, transport, *snapshots);
            continue;
        }

        if (const std::optional<TileMessagesPacket> tile_messages =
                TryDecodeTileMessages(packet->bytes.data(), packet->size)) {
            HandleTileMessagesAsPeer(state, *tile_messages);
            continue;
        }

        if (const std::optional<FluidCellMessagesPacket> fluid_messages =
                TryDecodeFluidCellMessages(packet->bytes.data(), packet->size)) {
            HandleFluidCellMessagesAsPeer(state, *fluid_messages);
            continue;
        }

        if (const std::optional<StageLightMessagesPacket> stage_light_messages =
                TryDecodeStageLightMessages(packet->bytes.data(), packet->size)) {
            HandleStageLightMessagesAsPeer(state, *stage_light_messages);
            continue;
        }

        if (const std::optional<EntitySpawnedMessagesPacket> entity_messages =
                TryDecodeEntitySpawnedMessages(packet->bytes.data(), packet->size)) {
            HandleEntitySpawnedMessagesAsPeer(state, *entity_messages);
            continue;
        }

        if (const std::optional<EntityDamageMessagesPacket> entity_messages =
                TryDecodeEntityDamageMessages(packet->bytes.data(), packet->size)) {
            HandleEntityDamageMessagesAsPeer(state, *entity_messages);
            continue;
        }

        if (const std::optional<EntityStateMessagesPacket> entity_messages =
                TryDecodeEntityStateMessages(packet->bytes.data(), packet->size)) {
            HandleEntityStateMessagesAsPeer(state, *entity_messages);
            continue;
        }

        if (const std::optional<EntityCarryMessagesPacket> entity_messages =
                TryDecodeEntityCarryMessages(packet->bytes.data(), packet->size)) {
            HandleEntityCarryMessagesAsPeer(state, *entity_messages);
            continue;
        }

        if (const std::optional<EntityLifecycleMessagesPacket> entity_messages =
                TryDecodeEntityLifecycleMessages(packet->bytes.data(), packet->size)) {
            HandleEntityLifecycleMessagesAsPeer(state, *entity_messages);
            continue;
        }

        if (const std::optional<PlayerStateMessagesPacket> player_state_messages =
                TryDecodePlayerStateMessages(packet->bytes.data(), packet->size)) {
            HandlePlayerStateMessagesAsPeer(state, *player_state_messages);
            continue;
        }

        if (const std::optional<RunStateMessagesPacket> run_state_messages =
                TryDecodeRunStateMessages(packet->bytes.data(), packet->size)) {
            HandleRunStateMessagesAsPeer(state, *run_state_messages);
            continue;
        }

        if (const std::optional<PresentationCommandMessagesPacket> presentation_messages =
                TryDecodePresentationCommandMessages(packet->bytes.data(), packet->size)) {
            HandlePresentationCommandMessagesAsPeer(state, *presentation_messages);
            continue;
        }

        if (const std::optional<ActionRequestAckPacket> action_ack =
                TryDecodeActionRequestAck(packet->bytes.data(), packet->size)) {
            HandleActionRequestAckAsPeer(state, *action_ack);
            continue;
        }
    }
}

} // namespace splonks::network

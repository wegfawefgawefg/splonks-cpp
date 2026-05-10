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

        if (const std::optional<DurableEventAckPacket> ack =
                TryDecodeDurableEventAck(packet->bytes.data(), packet->size)) {
            HandleDurableEventAckAsCoordinator(state, transport, packet->endpoint, *ack);
            continue;
        }

        if (const std::optional<PlayerSnapshotsPacket> snapshots =
                TryDecodePlayerSnapshots(packet->bytes.data(), packet->size)) {
            ApplyPlayerSnapshots(state, graphics, transport, *snapshots);
            RelaySnapshotsToOtherRemotes(transport, packet->endpoint, *snapshots);
            continue;
        }

        if (const std::optional<ActionRequestEventsPacket> action_requests =
                TryDecodeActionRequestEvents(packet->bytes.data(), packet->size)) {
            HandleActionRequestEventsAsCoordinator(state, transport, packet->endpoint, *action_requests);
            continue;
        }

        if (const std::optional<PresentationCommandEventsPacket> presentation_events =
                TryDecodePresentationCommandEvents(packet->bytes.data(), packet->size)) {
            HandlePresentationCommandEventsAsCoordinator(state, *presentation_events);
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

        if (const std::optional<TileEventsPacket> tile_events =
                TryDecodeTileEvents(packet->bytes.data(), packet->size)) {
            HandleTileEventsAsPeer(state, *tile_events);
            continue;
        }

        if (const std::optional<FluidCellEventsPacket> fluid_events =
                TryDecodeFluidCellEvents(packet->bytes.data(), packet->size)) {
            HandleFluidCellEventsAsPeer(state, *fluid_events);
            continue;
        }

        if (const std::optional<EntitySpawnedEventsPacket> entity_events =
                TryDecodeEntitySpawnedEvents(packet->bytes.data(), packet->size)) {
            HandleEntitySpawnedEventsAsPeer(state, *entity_events);
            continue;
        }

        if (const std::optional<EntityDamageEventsPacket> entity_events =
                TryDecodeEntityDamageEvents(packet->bytes.data(), packet->size)) {
            HandleEntityDamageEventsAsPeer(state, *entity_events);
            continue;
        }

        if (const std::optional<EntityStateEventsPacket> entity_events =
                TryDecodeEntityStateEvents(packet->bytes.data(), packet->size)) {
            HandleEntityStateEventsAsPeer(state, *entity_events);
            continue;
        }

        if (const std::optional<EntityCarryEventsPacket> entity_events =
                TryDecodeEntityCarryEvents(packet->bytes.data(), packet->size)) {
            HandleEntityCarryEventsAsPeer(state, *entity_events);
            continue;
        }

        if (const std::optional<EntityLifecycleEventsPacket> entity_events =
                TryDecodeEntityLifecycleEvents(packet->bytes.data(), packet->size)) {
            HandleEntityLifecycleEventsAsPeer(state, *entity_events);
            continue;
        }

        if (const std::optional<PlayerStateEventsPacket> player_state_events =
                TryDecodePlayerStateEvents(packet->bytes.data(), packet->size)) {
            HandlePlayerStateEventsAsPeer(state, *player_state_events);
            continue;
        }

        if (const std::optional<RunStateEventsPacket> run_state_events =
                TryDecodeRunStateEvents(packet->bytes.data(), packet->size)) {
            HandleRunStateEventsAsPeer(state, *run_state_events);
            continue;
        }

        if (const std::optional<PresentationCommandEventsPacket> presentation_events =
                TryDecodePresentationCommandEvents(packet->bytes.data(), packet->size)) {
            HandlePresentationCommandEventsAsPeer(state, *presentation_events);
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

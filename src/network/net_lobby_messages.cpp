#include "network/net_lobby_internal.hpp"

#include "network/net_ids.hpp"
#include "network/net_world_snapshot.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <variant>

namespace splonks::network {

namespace {

void RemovePendingOutboundMessage(NetSessionState& session, NetMessageId message_id) {
    session.pending_outbound_messages.erase(
        std::remove_if(
            session.pending_outbound_messages.begin(),
            session.pending_outbound_messages.end(),
            [message_id](const NetMessage& message) { return message.header.message_id == message_id; }
        ),
        session.pending_outbound_messages.end()
    );
}

bool IsOneShotTransientCoordinatorMessage(const NetMessage& message) {
    return message.header.coordinator_order == 0 &&
           (message.type == NetMessageType::EntityStatePatched ||
            message.type == NetMessageType::FluidCellPatched);
}

std::optional<std::uint64_t> FirstRetainedDurableCoordinatorOrder(
    const NetSessionState& session
) {
    std::optional<std::uint64_t> first_order;
    for (const NetMessage& message : session.ordered_messages) {
        const std::uint64_t order = message.header.coordinator_order;
        if (order == 0) {
            continue;
        }
        if (!first_order.has_value() || order < *first_order) {
            first_order = order;
        }
    }
    return first_order;
}

bool RemoteNeedsSameStageResync(
    const NetRemoteEndpoint& remote,
    const std::optional<std::uint64_t>& first_retained_order
) {
    if (!first_retained_order.has_value()) {
        return false;
    }
    const std::uint64_t next_remote_order = remote.highest_acked_coordinator_order + 1;
    return next_remote_order < *first_retained_order;
}

StageSyncPacket MakeSameStageForceResyncPacket(
    const State& state,
    std::uint64_t snapshot_start_order
) {
    StageSyncPacket packet;
    packet.stage_instance_id = state.net_session.stage_instance_id;
    packet.stage_seed = state.net_session.stage_seed;
    packet.snapshot_start_coordinator_order = snapshot_start_order;
    packet.force_resync = 1;
    WriteFixedString(state.net_session.quest_id, packet.quest_id);
    WriteFixedString(state.net_session.quest_stage_id, packet.quest_stage_id);
    return packet;
}

void SendSameStageForceResync(
    State& state,
    NetTransportRuntime& transport,
    const NetRemoteEndpoint& remote
) {
    if (remote.pending_resync_start_order == 0) {
        return;
    }
    SendEncodedPacket(
        transport,
        remote.endpoint,
        EncodeStageSync(MakeSameStageForceResyncPacket(
            state,
            remote.pending_resync_start_order
        ))
    );
}

void EnsureResyncForRemotesBehindRetainedHistory(
    State& state,
    NetTransportRuntime& transport
) {
    if (transport.remotes.empty()) {
        return;
    }

    const std::optional<std::uint64_t> first_retained_order =
        FirstRetainedDurableCoordinatorOrder(state.net_session);
    bool needs_snapshot = false;
    for (const NetRemoteEndpoint& remote : transport.remotes) {
        if (RemoteNeedsSameStageResync(remote, first_retained_order)) {
            needs_snapshot = true;
            break;
        }
    }
    if (!needs_snapshot) {
        return;
    }

    const std::uint64_t snapshot_start_order = state.net_session.next_coordinator_order;
    EnqueueWorldSnapshotMessages(state);

    for (NetRemoteEndpoint& remote : transport.remotes) {
        if (!RemoteNeedsSameStageResync(remote, first_retained_order)) {
            continue;
        }
        remote.pending_resync_start_order = snapshot_start_order;
        remote.highest_acked_coordinator_order =
            snapshot_start_order > 0 ? snapshot_start_order - 1 : 0;
        SendSameStageForceResync(state, transport, remote);
    }
}

void SendPendingSameStageResyncs(State& state, NetTransportRuntime& transport) {
    for (NetRemoteEndpoint& remote : transport.remotes) {
        if (remote.pending_resync_start_order == 0) {
            continue;
        }
        if (remote.highest_acked_coordinator_order >= remote.pending_resync_start_order) {
            remote.pending_resync_start_order = 0;
            continue;
        }
        SendSameStageForceResync(state, transport, remote);
    }
}

void PruneSentTransientCoordinatorMessages(NetSessionState& session) {
    session.ordered_messages.erase(
        std::remove_if(
            session.ordered_messages.begin(),
            session.ordered_messages.end(),
            [](const NetMessage& message) { return IsOneShotTransientCoordinatorMessage(message); }
        ),
        session.ordered_messages.end()
    );
}

bool HasQueuedOrAppliedMessage(const NetSessionState& session, NetMessageId message_id) {
    if (session.HasAppliedMessage(message_id)) {
        return true;
    }
    for (const NetMessage& message : session.pending_outbound_messages) {
        if (message.header.message_id == message_id) {
            return true;
        }
    }
    for (const NetMessage& message : session.ordered_messages) {
        if (message.header.message_id == message_id) {
            return true;
        }
    }
    return false;
}

void NoteCoordinatorOrderApplied(NetSessionState& session, const NetMessage& message) {
    session.MarkCoordinatorOrderApplied(message);
}

void MarkOutboundEchoMessageApplied(NetSessionState& session, const NetMessage& message) {
    RemovePendingOutboundMessage(session, message.header.message_id);
    if (session.MarkMessageApplied(message.header.message_id)) {
        NoteCoordinatorOrderApplied(session, message);
        session.AddMessageLog(NetMessageLogPhase::SkippedLocalApply, message);
    }
}

NetEntityId CanonicalizeIncomingEntityId(const NetSessionState& session, NetEntityId entity_id) {
    if (entity_id == kInvalidNetEntityId || IsPlayerNetEntityId(entity_id)) {
        return entity_id;
    }
    return session.ResolveEntityIdAlias(entity_id);
}

void CanonicalizeIncomingMessageEntityIds(NetSessionState& session, NetMessage& message) {
    if (EntitySpawnedMessage* const payload = std::get_if<EntitySpawnedMessage>(&message.payload)) {
        payload->entity_id = CanonicalizeIncomingEntityId(session, payload->entity_id);
        payload->held_by_id = CanonicalizeIncomingEntityId(session, payload->held_by_id);
        return;
    }
    if (EntityHeldMessage* const payload = std::get_if<EntityHeldMessage>(&message.payload)) {
        payload->holder_id = CanonicalizeIncomingEntityId(session, payload->holder_id);
        payload->held_id = CanonicalizeIncomingEntityId(session, payload->held_id);
        return;
    }
    if (EntityDroppedMessage* const payload = std::get_if<EntityDroppedMessage>(&message.payload)) {
        payload->entity_id = CanonicalizeIncomingEntityId(session, payload->entity_id);
        return;
    }
    if (EntityThrownMessage* const payload = std::get_if<EntityThrownMessage>(&message.payload)) {
        payload->entity_id = CanonicalizeIncomingEntityId(session, payload->entity_id);
        payload->thrower_id = CanonicalizeIncomingEntityId(session, payload->thrower_id);
        return;
    }
    if (EntityDamagedMessage* const payload = std::get_if<EntityDamagedMessage>(&message.payload)) {
        payload->entity_id = CanonicalizeIncomingEntityId(session, payload->entity_id);
        payload->source_entity_id = CanonicalizeIncomingEntityId(session, payload->source_entity_id);
        return;
    }
    if (EntityStatePatchedMessage* const payload = std::get_if<EntityStatePatchedMessage>(&message.payload)) {
        payload->entity_id = CanonicalizeIncomingEntityId(session, payload->entity_id);
        payload->source_entity_id = CanonicalizeIncomingEntityId(session, payload->source_entity_id);
        payload->entity_a_id = CanonicalizeIncomingEntityId(session, payload->entity_a_id);
        payload->holding_id = CanonicalizeIncomingEntityId(session, payload->holding_id);
        payload->held_by_id = CanonicalizeIncomingEntityId(session, payload->held_by_id);
        payload->back_id = CanonicalizeIncomingEntityId(session, payload->back_id);
        payload->buyable_shop_owner_id = CanonicalizeIncomingEntityId(session, payload->buyable_shop_owner_id);
        return;
    }
    if (PlayerStatePatchedMessage* const payload = std::get_if<PlayerStatePatchedMessage>(&message.payload)) {
        payload->player_entity_id = CanonicalizeIncomingEntityId(session, payload->player_entity_id);
        return;
    }
    if (PresentationCommandMessage* const payload = std::get_if<PresentationCommandMessage>(&message.payload)) {
        payload->source_entity_id = CanonicalizeIncomingEntityId(session, payload->source_entity_id);
        payload->target_entity_id = CanonicalizeIncomingEntityId(session, payload->target_entity_id);
        return;
    }
}

bool EndpointOwnsPlayer(
    const NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    PlayerId player_id
) {
    for (const NetRemoteEndpoint& remote : transport.remotes) {
        if (!EndpointsEqual(remote.endpoint, endpoint)) {
            continue;
        }
        return std::find(remote.player_ids.begin(), remote.player_ids.end(), player_id) !=
               remote.player_ids.end();
    }
    return false;
}

bool IsTransientEntityStatePatch(const NetMessage& message) {
    if (message.type != NetMessageType::EntityStatePatched) {
        return false;
    }
    const auto* const payload = std::get_if<EntityStatePatchedMessage>(&message.payload);
    return payload == nullptr || payload->source_entity_id == kInvalidNetEntityId;
}

} // namespace

void SendPendingPeerMessagesToCoordinator(State& state, NetTransportRuntime& transport) {
    if (state.net_session.pending_outbound_messages.empty()) {
        return;
    }
    SendPresentationCommandMessages(
        transport,
        transport.coordinator_endpoint,
        state.net_session.pending_outbound_messages
    );
}

void SendOrderedMessagesToAllRemotes(State& state, NetTransportRuntime& transport) {
    EnsureResyncForRemotesBehindRetainedHistory(state, transport);
    SendPendingSameStageResyncs(state, transport);

    if (state.net_session.ordered_messages.empty()) {
        return;
    }
    for (const NetRemoteEndpoint& remote : transport.remotes) {
        std::vector<NetMessage> unacked_messages;
        unacked_messages.reserve(state.net_session.ordered_messages.size());
        for (const NetMessage& message : state.net_session.ordered_messages) {
            if ((message.header.coordinator_order == 0 &&
                 (IsReplicatedEntityStateMessage(message) || IsReplicatedFluidCellMessage(message))) ||
                message.header.coordinator_order > remote.highest_acked_coordinator_order) {
                unacked_messages.push_back(message);
            }
        }
        if (unacked_messages.empty()) {
            continue;
        }
        SendTileMessages(transport, remote.endpoint, unacked_messages);
        SendFluidCellMessages(transport, remote.endpoint, unacked_messages);
        SendStageLightMessages(transport, remote.endpoint, unacked_messages);
        SendEntitySpawnedMessages(transport, remote.endpoint, unacked_messages);
        SendEntityDamageMessages(transport, remote.endpoint, unacked_messages);
        SendEntityStateMessages(transport, remote.endpoint, unacked_messages);
        SendEntityCarryMessages(transport, remote.endpoint, unacked_messages);
        SendEntityLifecycleMessages(transport, remote.endpoint, unacked_messages);
        SendPlayerStateMessages(transport, remote.endpoint, unacked_messages);
        SendRunStateMessages(transport, remote.endpoint, unacked_messages);
        SendPresentationCommandMessages(transport, remote.endpoint, unacked_messages);
    }
    PruneSentTransientCoordinatorMessages(state.net_session);
}

void PruneAckedOrderedMessages(State& state, const NetTransportRuntime& transport) {
    if (state.net_session.ordered_messages.empty() || transport.remotes.empty()) {
        return;
    }

    std::uint64_t lowest_remote_ack = std::numeric_limits<std::uint64_t>::max();
    for (const NetRemoteEndpoint& remote : transport.remotes) {
        lowest_remote_ack = std::min(
            lowest_remote_ack,
            remote.highest_acked_coordinator_order
        );
    }

    if (lowest_remote_ack == 0) {
        return;
    }

    state.net_session.ordered_messages.erase(
        std::remove_if(
            state.net_session.ordered_messages.begin(),
            state.net_session.ordered_messages.end(),
            [lowest_remote_ack](const NetMessage& message) {
                return message.header.coordinator_order != 0 &&
                       message.header.coordinator_order <= lowest_remote_ack;
            }
        ),
        state.net_session.ordered_messages.end()
    );
}

void SendDurableMessageAckToCoordinator(State& state, NetTransportRuntime& transport) {
    if (state.net_session.highest_applied_coordinator_order == 0) {
        return;
    }

    DurableMessageAckPacket ack;
    ack.stage_instance_id = state.net_session.stage_instance_id;
    ack.player_id = state.net_session.local_player_id;
    ack.highest_applied_coordinator_order = state.net_session.highest_applied_coordinator_order;
    SendEncodedPacket(
        transport,
        transport.coordinator_endpoint,
        EncodeDurableMessageAck(ack)
    );
}

void HandleDurableMessageAckAsCoordinator(
    State& state,
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const DurableMessageAckPacket& ack
) {
    if (ack.stage_instance_id != state.net_session.stage_instance_id ||
        ack.player_id == kInvalidPlayerId) {
        return;
    }

    for (NetRemoteEndpoint& remote : transport.remotes) {
        if (!EndpointsEqual(remote.endpoint, endpoint)) {
            continue;
        }
        if (!EndpointOwnsPlayer(transport, endpoint, ack.player_id)) {
            return;
        }
        remote.highest_acked_coordinator_order = std::max(
            remote.highest_acked_coordinator_order,
            ack.highest_applied_coordinator_order
        );
        PruneAckedOrderedMessages(state, transport);
        return;
    }
}

void HandleTileMessagesAsPeer(State& state, const TileMessagesPacket& packet) {
    for (std::uint32_t i = 0; i < packet.message_count; ++i) {
        const TileMessageEntry& entry = packet.messages[i];
        if (entry.stage_instance_id != state.net_session.stage_instance_id ||
            entry.message_id == kInvalidNetMessageId) {
            continue;
        }
        if (entry.source_player_id != state.net_session.coordinator_player_id) {
            continue;
        }
        if (HasQueuedOrAppliedMessage(state.net_session, entry.message_id)) {
            continue;
        }
        NetMessage message = MakeTileMessage(entry);
        if (message.type != NetMessageType::None) {
            state.net_session.EnqueueOrderedMessage(message);
        }
    }
}

void HandleFluidCellMessagesAsPeer(State& state, const FluidCellMessagesPacket& packet) {
    for (std::uint32_t i = 0; i < packet.message_count; ++i) {
        const FluidCellMessageEntry& entry = packet.messages[i];
        if (entry.stage_instance_id != state.net_session.stage_instance_id ||
            entry.message_id == kInvalidNetMessageId) {
            continue;
        }
        if (entry.source_player_id != state.net_session.coordinator_player_id) {
            continue;
        }
        if (HasQueuedOrAppliedMessage(state.net_session, entry.message_id)) {
            continue;
        }
        NetMessage message = MakeFluidCellMessage(entry);
        if (message.header.coordinator_order == 0) {
            state.net_session.EnqueueTransientMessage(message);
        } else {
            state.net_session.EnqueueOrderedMessage(message);
        }
    }
}

void HandleStageLightMessagesAsPeer(State& state, const StageLightMessagesPacket& packet) {
    for (std::uint32_t i = 0; i < packet.message_count; ++i) {
        const StageLightMessageEntry& entry = packet.messages[i];
        if (entry.stage_instance_id != state.net_session.stage_instance_id ||
            entry.message_id == kInvalidNetMessageId) {
            continue;
        }
        if (entry.source_player_id != state.net_session.coordinator_player_id) {
            continue;
        }
        if (HasQueuedOrAppliedMessage(state.net_session, entry.message_id)) {
            continue;
        }
        NetMessage message = MakeStageLightMessage(entry);
        if (message.type != NetMessageType::None) {
            state.net_session.EnqueueOrderedMessage(message);
        }
    }
}

void HandleEntitySpawnedMessagesAsPeer(State& state, const EntitySpawnedMessagesPacket& packet) {
    for (std::uint32_t i = 0; i < packet.message_count; ++i) {
        const EntitySpawnedMessageEntry& entry = packet.messages[i];
        if (entry.stage_instance_id != state.net_session.stage_instance_id ||
            entry.message_id == kInvalidNetMessageId) {
            continue;
        }
        if (entry.source_player_id != state.net_session.coordinator_player_id) {
            continue;
        }
        if (HasQueuedOrAppliedMessage(state.net_session, entry.message_id)) {
            continue;
        }
        state.net_session.EnqueueOrderedMessage(MakeEntitySpawnedMessage(entry));
    }
}

void HandleEntityDamageMessagesAsPeer(State& state, const EntityDamageMessagesPacket& packet) {
    for (std::uint32_t i = 0; i < packet.message_count; ++i) {
        const EntityDamageMessageEntry& entry = packet.messages[i];
        if (entry.stage_instance_id != state.net_session.stage_instance_id ||
            entry.message_id == kInvalidNetMessageId) {
            continue;
        }
        if (entry.source_player_id != state.net_session.coordinator_player_id) {
            continue;
        }
        if (HasQueuedOrAppliedMessage(state.net_session, entry.message_id)) {
            continue;
        }
        state.net_session.EnqueueOrderedMessage(MakeEntityDamageMessage(entry));
    }
}

void HandleEntityStateMessagesAsPeer(State& state, const EntityStateMessagesPacket& packet) {
    for (std::uint32_t i = 0; i < packet.message_count; ++i) {
        const EntityStateMessageEntry& entry = packet.messages[i];
        if (entry.stage_instance_id != state.net_session.stage_instance_id ||
            entry.message_id == kInvalidNetMessageId) {
            continue;
        }
        if (entry.source_player_id != state.net_session.coordinator_player_id) {
            continue;
        }
        if (HasQueuedOrAppliedMessage(state.net_session, entry.message_id)) {
            continue;
        }
        NetMessage message = MakeEntityStateMessage(entry);
        if (IsTransientEntityStatePatch(message)) {
            state.net_session.EnqueueTransientMessage(message);
        } else {
            state.net_session.EnqueueOrderedMessage(message);
        }
    }
}

void HandleEntityCarryMessagesAsPeer(State& state, const EntityCarryMessagesPacket& packet) {
    for (std::uint32_t i = 0; i < packet.message_count; ++i) {
        const EntityCarryMessageEntry& entry = packet.messages[i];
        if (entry.stage_instance_id != state.net_session.stage_instance_id ||
            entry.message_id == kInvalidNetMessageId) {
            continue;
        }
        if (entry.source_player_id != state.net_session.coordinator_player_id) {
            continue;
        }
        if (HasQueuedOrAppliedMessage(state.net_session, entry.message_id)) {
            continue;
        }
        NetMessage message = MakeEntityCarryMessage(entry);
        if (message.type != NetMessageType::None) {
            state.net_session.EnqueueOrderedMessage(message);
        }
    }
}

void HandleEntityLifecycleMessagesAsPeer(State& state, const EntityLifecycleMessagesPacket& packet) {
    for (std::uint32_t i = 0; i < packet.message_count; ++i) {
        const EntityLifecycleMessageEntry& entry = packet.messages[i];
        if (entry.stage_instance_id != state.net_session.stage_instance_id ||
            entry.message_id == kInvalidNetMessageId) {
            continue;
        }
        if (entry.source_player_id != state.net_session.coordinator_player_id) {
            continue;
        }
        if (HasQueuedOrAppliedMessage(state.net_session, entry.message_id)) {
            continue;
        }
        NetMessage message = MakeEntityLifecycleMessage(entry);
        if (message.type != NetMessageType::None) {
            state.net_session.EnqueueOrderedMessage(message);
        }
    }
}

void HandlePlayerStateMessagesAsPeer(State& state, const PlayerStateMessagesPacket& packet) {
    for (std::uint32_t i = 0; i < packet.message_count; ++i) {
        const PlayerStateMessageEntry& entry = packet.messages[i];
        if (entry.stage_instance_id != state.net_session.stage_instance_id ||
            entry.message_id == kInvalidNetMessageId) {
            continue;
        }
        if (entry.source_player_id != state.net_session.coordinator_player_id) {
            continue;
        }
        if (HasQueuedOrAppliedMessage(state.net_session, entry.message_id)) {
            continue;
        }
        state.net_session.EnqueueOrderedMessage(MakePlayerStateMessage(entry));
    }
}

void HandleRunStateMessagesAsPeer(State& state, const RunStateMessagesPacket& packet) {
    for (std::uint32_t i = 0; i < packet.message_count; ++i) {
        const RunStateMessageEntry& entry = packet.messages[i];
        if (entry.stage_instance_id != state.net_session.stage_instance_id ||
            entry.message_id == kInvalidNetMessageId) {
            continue;
        }
        if (entry.source_player_id != state.net_session.coordinator_player_id) {
            continue;
        }
        if (HasQueuedOrAppliedMessage(state.net_session, entry.message_id)) {
            continue;
        }
        state.net_session.EnqueueOrderedMessage(MakeRunStateMessage(entry));
    }
}

void HandlePresentationCommandMessagesAsCoordinator(State& state, const PresentationCommandMessagesPacket& packet) {
    for (std::uint32_t i = 0; i < packet.message_count; ++i) {
        NetMessage message = MakePresentationCommandMessage(packet.messages[i]);
        if (message.header.stage_instance_id != state.net_session.stage_instance_id ||
            message.header.message_id == kInvalidNetMessageId ||
            HasQueuedOrAppliedMessage(state.net_session, message.header.message_id)) {
            continue;
        }
        CanonicalizeIncomingMessageEntityIds(state.net_session, message);
        message.header.coordinator_order = state.net_session.next_coordinator_order++;
        state.net_session.EnqueueOrderedMessage(message);
    }
}

void HandlePresentationCommandMessagesAsPeer(State& state, const PresentationCommandMessagesPacket& packet) {
    for (std::uint32_t i = 0; i < packet.message_count; ++i) {
        const PresentationCommandMessageEntry& entry = packet.messages[i];
        if (entry.stage_instance_id != state.net_session.stage_instance_id ||
            entry.message_id == kInvalidNetMessageId) {
            continue;
        }
        if (entry.source_player_id == state.net_session.local_player_id) {
            MarkOutboundEchoMessageApplied(state.net_session, MakePresentationCommandMessage(entry));
            continue;
        }
        if (HasQueuedOrAppliedMessage(state.net_session, entry.message_id)) {
            continue;
        }
        state.net_session.EnqueueOrderedMessage(MakePresentationCommandMessage(entry));
    }
}

} // namespace splonks::network

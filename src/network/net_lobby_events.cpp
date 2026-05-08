#include "network/net_lobby_internal.hpp"

#include "gameplay_messages.hpp"
#include "network/net_ids.hpp"
#include "world_ops.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <variant>

namespace splonks::network {

namespace {

void RemovePendingOutboundEvent(NetSessionState& session, NetEventId event_id) {
    session.pending_outbound_events.erase(
        std::remove_if(
            session.pending_outbound_events.begin(),
            session.pending_outbound_events.end(),
            [event_id](const NetEvent& event) { return event.header.event_id == event_id; }
        ),
        session.pending_outbound_events.end()
    );
}

void RemovePendingBreakTileRequestsForTile(NetSessionState& session, const IVec2& tile_pos) {
    session.pending_outbound_events.erase(
        std::remove_if(
            session.pending_outbound_events.begin(),
            session.pending_outbound_events.end(),
            [&](const NetEvent& event) {
                if (event.type != NetEventType::ActionRequest) {
                    return false;
                }
                const ActionRequestEvent* const payload =
                    std::get_if<ActionRequestEvent>(&event.payload);
                return payload != nullptr &&
                       payload->kind == NetActionKind::BreakTile &&
                       payload->tile_pos.x == tile_pos.x &&
                       payload->tile_pos.y == tile_pos.y;
            }
        ),
        session.pending_outbound_events.end()
    );
}

void RemovePendingEntityActionRequestsForResult(
    NetSessionState& session,
    NetEntityId source_entity_id,
    NetEntityId target_entity_id
) {
    session.pending_outbound_events.erase(
        std::remove_if(
            session.pending_outbound_events.begin(),
            session.pending_outbound_events.end(),
            [&](const NetEvent& event) {
                if (event.type != NetEventType::ActionRequest) {
                    return false;
                }
                const ActionRequestEvent* const payload =
                    std::get_if<ActionRequestEvent>(&event.payload);
                if (payload == nullptr ||
                    (payload->kind != NetActionKind::DamageEntity &&
                     payload->kind != NetActionKind::HitEntity)) {
                    return false;
                }
                return payload->source_entity_id == source_entity_id &&
                       payload->target_entity_id == target_entity_id;
            }
        ),
        session.pending_outbound_events.end()
    );
}

bool IsOneShotTransientCoordinatorEvent(const NetEvent& event) {
    return event.header.coordinator_order == 0 &&
           (event.type == NetEventType::EntityStatePatched ||
            event.type == NetEventType::FluidCellPatched);
}

void PruneSentTransientCoordinatorEvents(NetSessionState& session) {
    session.ordered_events.erase(
        std::remove_if(
            session.ordered_events.begin(),
            session.ordered_events.end(),
            [](const NetEvent& event) { return IsOneShotTransientCoordinatorEvent(event); }
        ),
        session.ordered_events.end()
    );
}

bool HasQueuedOrAppliedEvent(const NetSessionState& session, NetEventId event_id) {
    if (session.HasAppliedEvent(event_id)) {
        return true;
    }
    for (const NetEvent& event : session.pending_outbound_events) {
        if (event.header.event_id == event_id) {
            return true;
        }
    }
    for (const NetEvent& event : session.ordered_events) {
        if (event.header.event_id == event_id) {
            return true;
        }
    }
    return false;
}

void NoteCoordinatorOrderApplied(NetSessionState& session, const NetEvent& event) {
    session.MarkCoordinatorOrderApplied(event);
}

void MarkOutboundEchoEventApplied(NetSessionState& session, const NetEvent& event) {
    RemovePendingOutboundEvent(session, event.header.event_id);
    if (session.MarkEventApplied(event.header.event_id)) {
        NoteCoordinatorOrderApplied(session, event);
        session.AddEventLog(NetEventLogPhase::SkippedLocalApply, event);
    }
}

NetEntityId CanonicalizeIncomingEntityId(const NetSessionState& session, NetEntityId entity_id) {
    if (entity_id == kInvalidNetEntityId || IsPlayerNetEntityId(entity_id)) {
        return entity_id;
    }
    return session.ResolveEntityIdAlias(entity_id);
}

void CanonicalizeIncomingEventEntityIds(NetSessionState& session, NetEvent& event) {
    if (EntitySpawnedEvent* const payload = std::get_if<EntitySpawnedEvent>(&event.payload)) {
        payload->entity_id = CanonicalizeIncomingEntityId(session, payload->entity_id);
        payload->held_by_id = CanonicalizeIncomingEntityId(session, payload->held_by_id);
        return;
    }
    if (EntityHeldEvent* const payload = std::get_if<EntityHeldEvent>(&event.payload)) {
        payload->holder_id = CanonicalizeIncomingEntityId(session, payload->holder_id);
        payload->held_id = CanonicalizeIncomingEntityId(session, payload->held_id);
        return;
    }
    if (EntityDroppedEvent* const payload = std::get_if<EntityDroppedEvent>(&event.payload)) {
        payload->entity_id = CanonicalizeIncomingEntityId(session, payload->entity_id);
        return;
    }
    if (EntityThrownEvent* const payload = std::get_if<EntityThrownEvent>(&event.payload)) {
        payload->entity_id = CanonicalizeIncomingEntityId(session, payload->entity_id);
        payload->thrower_id = CanonicalizeIncomingEntityId(session, payload->thrower_id);
        return;
    }
    if (EntityDamagedEvent* const payload = std::get_if<EntityDamagedEvent>(&event.payload)) {
        payload->entity_id = CanonicalizeIncomingEntityId(session, payload->entity_id);
        payload->source_entity_id = CanonicalizeIncomingEntityId(session, payload->source_entity_id);
        return;
    }
    if (EntityStatePatchedEvent* const payload = std::get_if<EntityStatePatchedEvent>(&event.payload)) {
        payload->entity_id = CanonicalizeIncomingEntityId(session, payload->entity_id);
        payload->source_entity_id = CanonicalizeIncomingEntityId(session, payload->source_entity_id);
        payload->entity_a_id = CanonicalizeIncomingEntityId(session, payload->entity_a_id);
        payload->holding_id = CanonicalizeIncomingEntityId(session, payload->holding_id);
        payload->held_by_id = CanonicalizeIncomingEntityId(session, payload->held_by_id);
        payload->back_id = CanonicalizeIncomingEntityId(session, payload->back_id);
        payload->buyable_shop_owner_id = CanonicalizeIncomingEntityId(session, payload->buyable_shop_owner_id);
        return;
    }
    if (PlayerStatePatchedEvent* const payload = std::get_if<PlayerStatePatchedEvent>(&event.payload)) {
        payload->player_entity_id = CanonicalizeIncomingEntityId(session, payload->player_entity_id);
        return;
    }
    if (PresentationCommandEvent* const payload = std::get_if<PresentationCommandEvent>(&event.payload)) {
        payload->source_entity_id = CanonicalizeIncomingEntityId(session, payload->source_entity_id);
        payload->target_entity_id = CanonicalizeIncomingEntityId(session, payload->target_entity_id);
        return;
    }
    if (ActionRequestEvent* const payload = std::get_if<ActionRequestEvent>(&event.payload)) {
        payload->source_entity_id = CanonicalizeIncomingEntityId(session, payload->source_entity_id);
        payload->target_entity_id = CanonicalizeIncomingEntityId(session, payload->target_entity_id);
    }
}

GameplayActionKind ToGameplayActionKind(NetActionKind kind) {
    switch (kind) {
    case NetActionKind::UseTool:
        return GameplayActionKind::UseTool;
    case NetActionKind::PickupEntity:
        return GameplayActionKind::PickupEntity;
    case NetActionKind::DropEntity:
        return GameplayActionKind::DropEntity;
    case NetActionKind::ThrowEntity:
        return GameplayActionKind::ThrowEntity;
    case NetActionKind::UseHeldEntity:
        return GameplayActionKind::UseHeldEntity;
    case NetActionKind::UseBackEntity:
        return GameplayActionKind::UseBackEntity;
    case NetActionKind::PutHeldEntityOnBack:
        return GameplayActionKind::PutHeldEntityOnBack;
    case NetActionKind::TakeOffBackEntity:
        return GameplayActionKind::TakeOffBackEntity;
    case NetActionKind::InteractEntity:
        return GameplayActionKind::InteractEntity;
    case NetActionKind::CollectEntity:
        return GameplayActionKind::CollectEntity;
    case NetActionKind::PushEntity:
        return GameplayActionKind::PushEntity;
    case NetActionKind::BreakTile:
        return GameplayActionKind::BreakTile;
    case NetActionKind::DamageEntity:
        return GameplayActionKind::DamageEntity;
    case NetActionKind::HitEntity:
        return GameplayActionKind::HitEntity;
    case NetActionKind::None:
        return GameplayActionKind::None;
    }
    return GameplayActionKind::None;
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

bool IsTransientEntityStatePatch(const NetEvent& event) {
    if (event.type != NetEventType::EntityStatePatched) {
        return false;
    }
    const auto* const payload = std::get_if<EntityStatePatchedEvent>(&event.payload);
    return payload == nullptr || payload->source_entity_id == kInvalidNetEntityId;
}

} // namespace

void SendPendingPeerEventsToCoordinator(State& state, NetTransportRuntime& transport) {
    if (state.net_session.pending_outbound_events.empty()) {
        return;
    }
    SendActionRequestEvents(
        transport,
        transport.coordinator_endpoint,
        state.net_session.pending_outbound_events
    );
    SendPresentationCommandEvents(
        transport,
        transport.coordinator_endpoint,
        state.net_session.pending_outbound_events
    );
}

void SendActionRequestAck(
    State& state,
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetEventId>& event_ids
) {
    if (event_ids.empty()) {
        return;
    }

    ActionRequestAckPacket packet;
    packet.stage_instance_id = state.net_session.stage_instance_id;
    packet.coordinator_player_id = state.net_session.local_player_id;
    for (NetEventId event_id : event_ids) {
        if (event_id == kInvalidNetEventId) {
            continue;
        }
        if (packet.ack_count == packet.event_ids.size()) {
            SendEncodedPacket(transport, endpoint, EncodeActionRequestAck(packet));
            packet = ActionRequestAckPacket{};
            packet.stage_instance_id = state.net_session.stage_instance_id;
            packet.coordinator_player_id = state.net_session.local_player_id;
        }
        packet.event_ids[packet.ack_count++] = event_id;
    }
    if (packet.ack_count > 0) {
        SendEncodedPacket(transport, endpoint, EncodeActionRequestAck(packet));
    }
}

void SendOrderedEventsToAllRemotes(State& state, NetTransportRuntime& transport) {
    if (state.net_session.ordered_events.empty()) {
        return;
    }
    for (const NetRemoteEndpoint& remote : transport.remotes) {
        std::vector<NetEvent> unacked_events;
        unacked_events.reserve(state.net_session.ordered_events.size());
        for (const NetEvent& event : state.net_session.ordered_events) {
            if ((event.header.coordinator_order == 0 && IsReplicatedEntityStateEvent(event)) ||
                event.header.coordinator_order > remote.highest_acked_coordinator_order) {
                unacked_events.push_back(event);
            }
        }
        if (unacked_events.empty()) {
            continue;
        }
        SendTileEvents(transport, remote.endpoint, unacked_events);
        SendEntitySpawnedEvents(transport, remote.endpoint, unacked_events);
        SendEntityDamageEvents(transport, remote.endpoint, unacked_events);
        SendEntityStateEvents(transport, remote.endpoint, unacked_events);
        SendEntityCarryEvents(transport, remote.endpoint, unacked_events);
        SendEntityLifecycleEvents(transport, remote.endpoint, unacked_events);
        SendPlayerStateEvents(transport, remote.endpoint, unacked_events);
        SendRunStateEvents(transport, remote.endpoint, unacked_events);
        SendPresentationCommandEvents(transport, remote.endpoint, unacked_events);
    }
    PruneSentTransientCoordinatorEvents(state.net_session);
}

void PruneAckedOrderedEvents(State& state, const NetTransportRuntime& transport) {
    if (state.net_session.ordered_events.empty() || transport.remotes.empty()) {
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

    state.net_session.ordered_events.erase(
        std::remove_if(
            state.net_session.ordered_events.begin(),
            state.net_session.ordered_events.end(),
            [lowest_remote_ack](const NetEvent& event) {
                return event.header.coordinator_order != 0 &&
                       event.header.coordinator_order <= lowest_remote_ack;
            }
        ),
        state.net_session.ordered_events.end()
    );
}

void SendDurableEventAckToCoordinator(State& state, NetTransportRuntime& transport) {
    if (state.net_session.highest_applied_coordinator_order == 0) {
        return;
    }

    DurableEventAckPacket ack;
    ack.stage_instance_id = state.net_session.stage_instance_id;
    ack.player_id = state.net_session.local_player_id;
    ack.highest_applied_coordinator_order = state.net_session.highest_applied_coordinator_order;
    SendEncodedPacket(
        transport,
        transport.coordinator_endpoint,
        EncodeDurableEventAck(ack)
    );
}

void HandleDurableEventAckAsCoordinator(
    State& state,
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const DurableEventAckPacket& ack
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
        PruneAckedOrderedEvents(state, transport);
        return;
    }
}

void HandleTileEventsAsPeer(State& state, const TileEventsPacket& packet) {
    for (std::uint32_t i = 0; i < packet.event_count; ++i) {
        const TileEventEntry& entry = packet.events[i];
        if (entry.stage_instance_id != state.net_session.stage_instance_id ||
            entry.event_id == kInvalidNetEventId) {
            continue;
        }
        if (entry.source_player_id != state.net_session.coordinator_player_id) {
            continue;
        }
        if (static_cast<NetEventType>(entry.event_type) == NetEventType::TileBroken) {
            RemovePendingBreakTileRequestsForTile(
                state.net_session,
                IVec2::New(entry.tile_x, entry.tile_y)
            );
        }
        if (HasQueuedOrAppliedEvent(state.net_session, entry.event_id)) {
            continue;
        }
        NetEvent event = MakeTileEvent(entry);
        if (event.type != NetEventType::None) {
            state.net_session.EnqueueOrderedEvent(event);
        }
    }
}

void HandleFluidCellEventsAsPeer(State& state, const FluidCellEventsPacket& packet) {
    for (std::uint32_t i = 0; i < packet.event_count; ++i) {
        const FluidCellEventEntry& entry = packet.events[i];
        if (entry.stage_instance_id != state.net_session.stage_instance_id ||
            entry.event_id == kInvalidNetEventId) {
            continue;
        }
        if (entry.source_player_id != state.net_session.coordinator_player_id) {
            continue;
        }
        if (HasQueuedOrAppliedEvent(state.net_session, entry.event_id)) {
            continue;
        }
        state.net_session.EnqueueTransientEvent(MakeFluidCellEvent(entry));
    }
}

void HandleEntitySpawnedEventsAsPeer(State& state, const EntitySpawnedEventsPacket& packet) {
    for (std::uint32_t i = 0; i < packet.event_count; ++i) {
        const EntitySpawnedEventEntry& entry = packet.events[i];
        if (entry.stage_instance_id != state.net_session.stage_instance_id ||
            entry.event_id == kInvalidNetEventId) {
            continue;
        }
        if (entry.source_player_id != state.net_session.coordinator_player_id) {
            continue;
        }
        if (HasQueuedOrAppliedEvent(state.net_session, entry.event_id)) {
            continue;
        }
        state.net_session.EnqueueOrderedEvent(MakeEntitySpawnedEvent(entry));
    }
}

void HandleEntityDamageEventsAsPeer(State& state, const EntityDamageEventsPacket& packet) {
    for (std::uint32_t i = 0; i < packet.event_count; ++i) {
        const EntityDamageEventEntry& entry = packet.events[i];
        if (entry.stage_instance_id != state.net_session.stage_instance_id ||
            entry.event_id == kInvalidNetEventId) {
            continue;
        }
        if (entry.source_player_id != state.net_session.coordinator_player_id) {
            continue;
        }
        RemovePendingEntityActionRequestsForResult(
            state.net_session,
            entry.source_entity_id,
            entry.entity_id
        );
        if (HasQueuedOrAppliedEvent(state.net_session, entry.event_id)) {
            continue;
        }
        state.net_session.EnqueueOrderedEvent(MakeEntityDamageEvent(entry));
    }
}

void HandleEntityStateEventsAsPeer(State& state, const EntityStateEventsPacket& packet) {
    for (std::uint32_t i = 0; i < packet.event_count; ++i) {
        const EntityStateEventEntry& entry = packet.events[i];
        if (entry.stage_instance_id != state.net_session.stage_instance_id ||
            entry.event_id == kInvalidNetEventId) {
            continue;
        }
        if (entry.source_player_id != state.net_session.coordinator_player_id) {
            continue;
        }
        RemovePendingEntityActionRequestsForResult(
            state.net_session,
            entry.source_entity_id,
            entry.entity_id
        );
        if (HasQueuedOrAppliedEvent(state.net_session, entry.event_id)) {
            continue;
        }
        NetEvent event = MakeEntityStateEvent(entry);
        if (IsTransientEntityStatePatch(event)) {
            state.net_session.EnqueueTransientEvent(event);
        } else {
            state.net_session.EnqueueOrderedEvent(event);
        }
    }
}

void HandleEntityCarryEventsAsPeer(State& state, const EntityCarryEventsPacket& packet) {
    for (std::uint32_t i = 0; i < packet.event_count; ++i) {
        const EntityCarryEventEntry& entry = packet.events[i];
        if (entry.stage_instance_id != state.net_session.stage_instance_id ||
            entry.event_id == kInvalidNetEventId) {
            continue;
        }
        if (entry.source_player_id != state.net_session.coordinator_player_id) {
            continue;
        }
        if (HasQueuedOrAppliedEvent(state.net_session, entry.event_id)) {
            continue;
        }
        NetEvent event = MakeEntityCarryEvent(entry);
        if (event.type != NetEventType::None) {
            state.net_session.EnqueueOrderedEvent(event);
        }
    }
}

void HandleEntityLifecycleEventsAsPeer(State& state, const EntityLifecycleEventsPacket& packet) {
    for (std::uint32_t i = 0; i < packet.event_count; ++i) {
        const EntityLifecycleEventEntry& entry = packet.events[i];
        if (entry.stage_instance_id != state.net_session.stage_instance_id ||
            entry.event_id == kInvalidNetEventId) {
            continue;
        }
        if (entry.source_player_id != state.net_session.coordinator_player_id) {
            continue;
        }
        if (HasQueuedOrAppliedEvent(state.net_session, entry.event_id)) {
            continue;
        }
        NetEvent event = MakeEntityLifecycleEvent(entry);
        if (event.type != NetEventType::None) {
            state.net_session.EnqueueOrderedEvent(event);
        }
    }
}

void HandlePlayerStateEventsAsPeer(State& state, const PlayerStateEventsPacket& packet) {
    for (std::uint32_t i = 0; i < packet.event_count; ++i) {
        const PlayerStateEventEntry& entry = packet.events[i];
        if (entry.stage_instance_id != state.net_session.stage_instance_id ||
            entry.event_id == kInvalidNetEventId) {
            continue;
        }
        if (entry.source_player_id != state.net_session.coordinator_player_id) {
            continue;
        }
        if (HasQueuedOrAppliedEvent(state.net_session, entry.event_id)) {
            continue;
        }
        state.net_session.EnqueueOrderedEvent(MakePlayerStateEvent(entry));
    }
}

void HandleRunStateEventsAsPeer(State& state, const RunStateEventsPacket& packet) {
    for (std::uint32_t i = 0; i < packet.event_count; ++i) {
        const RunStateEventEntry& entry = packet.events[i];
        if (entry.stage_instance_id != state.net_session.stage_instance_id ||
            entry.event_id == kInvalidNetEventId) {
            continue;
        }
        if (entry.source_player_id != state.net_session.coordinator_player_id) {
            continue;
        }
        if (HasQueuedOrAppliedEvent(state.net_session, entry.event_id)) {
            continue;
        }
        state.net_session.EnqueueOrderedEvent(MakeRunStateEvent(entry));
    }
}

void HandlePresentationCommandEventsAsCoordinator(State& state, const PresentationCommandEventsPacket& packet) {
    for (std::uint32_t i = 0; i < packet.event_count; ++i) {
        NetEvent event = MakePresentationCommandEvent(packet.events[i]);
        if (event.header.stage_instance_id != state.net_session.stage_instance_id ||
            event.header.event_id == kInvalidNetEventId ||
            HasQueuedOrAppliedEvent(state.net_session, event.header.event_id)) {
            continue;
        }
        CanonicalizeIncomingEventEntityIds(state.net_session, event);
        event.header.coordinator_order = state.net_session.next_coordinator_order++;
        state.net_session.EnqueueOrderedEvent(event);
    }
}

void HandlePresentationCommandEventsAsPeer(State& state, const PresentationCommandEventsPacket& packet) {
    for (std::uint32_t i = 0; i < packet.event_count; ++i) {
        const PresentationCommandEventEntry& entry = packet.events[i];
        if (entry.stage_instance_id != state.net_session.stage_instance_id ||
            entry.event_id == kInvalidNetEventId) {
            continue;
        }
        if (entry.source_player_id == state.net_session.local_player_id) {
            MarkOutboundEchoEventApplied(state.net_session, MakePresentationCommandEvent(entry));
            continue;
        }
        if (HasQueuedOrAppliedEvent(state.net_session, entry.event_id)) {
            continue;
        }
        state.net_session.EnqueueOrderedEvent(MakePresentationCommandEvent(entry));
    }
}

void HandleActionRequestEventsAsCoordinator(
    State& state,
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const ActionRequestEventsPacket& packet
) {
    std::vector<NetEventId> ack_event_ids;
    ack_event_ids.reserve(packet.event_count);
    for (std::uint32_t i = 0; i < packet.event_count; ++i) {
        NetEvent event = MakeActionRequestEvent(packet.events[i]);
        if (event.header.stage_instance_id != state.net_session.stage_instance_id ||
            event.header.event_id == kInvalidNetEventId ||
            event.type != NetEventType::ActionRequest) {
            continue;
        }
        ack_event_ids.push_back(event.header.event_id);
        if (HasQueuedOrAppliedEvent(state.net_session, event.header.event_id)) {
            continue;
        }

        CanonicalizeIncomingEventEntityIds(state.net_session, event);
        const ActionRequestEvent* const payload = std::get_if<ActionRequestEvent>(&event.payload);
        if (payload == nullptr || payload->kind == NetActionKind::None) {
            continue;
        }
        const std::optional<VID> source_vid = state.net_session.FindLocalVid(payload->source_entity_id);
        const std::optional<VID> target_vid = state.net_session.FindLocalVid(payload->target_entity_id);

        world_ops::QueuePendingGameplayAction(
            state,
            GameplayActionRequested{
                .kind = ToGameplayActionKind(payload->kind),
                .source_vid = source_vid,
                .target_vid = target_vid,
                .tile_pos = payload->tile_pos,
                .direction = payload->direction,
                .world_pos = payload->world_pos,
                .velocity = payload->velocity,
                .damage_type = payload->damage_type,
                .projectile_contact_damage_type = payload->projectile_contact_damage_type,
                .amount = payload->amount,
                .projectile_contact_damage_amount = payload->projectile_contact_damage_amount,
                .thrown_immunity_timer = payload->thrown_immunity_timer,
                .projectile_contact_duration = payload->projectile_contact_duration,
                .clear_velocity = payload->clear_velocity,
                .clear_acceleration = payload->clear_acceleration,
                .knockback_on_no_damage = payload->knockback_on_no_damage,
                .param_a = payload->param_a,
                .param_b = payload->param_b,
            }
        );
        if (state.net_session.MarkEventApplied(event.header.event_id)) {
            state.net_session.AddEventLog(NetEventLogPhase::Applied, event);
        }
    }
    SendActionRequestAck(state, transport, endpoint, ack_event_ids);
}

void HandleActionRequestAckAsPeer(State& state, const ActionRequestAckPacket& packet) {
    if (packet.stage_instance_id != state.net_session.stage_instance_id ||
        packet.coordinator_player_id != state.net_session.coordinator_player_id) {
        return;
    }

    for (std::uint32_t i = 0; i < packet.ack_count; ++i) {
        RemovePendingOutboundEvent(state.net_session, packet.event_ids[i]);
    }
}

} // namespace splonks::network

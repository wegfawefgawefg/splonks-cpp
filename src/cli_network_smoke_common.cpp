#include "cli_network_smoke_internal.hpp"

#include "audio.hpp"
#include "entity.hpp"
#include "entity/archetype.hpp"
#include "entity_tool_inventory.hpp"
#include "graphics.hpp"
#include "network/net_event.hpp"
#include "network/net_event_apply.hpp"
#include "network/net_ids.hpp"
#include "network/net_lobby_internal.hpp"
#include "network/net_progression.hpp"
#include "quest_stage_loader.hpp"
#include "raw_frame_data.hpp"
#include "state.hpp"
#include "tile_source_data.hpp"
#include "tools/tool_archetype.hpp"
#include "world_ops.hpp"

#include <algorithm>
#include <iostream>
#include <optional>
#include <variant>
#include <vector>

namespace splonks {

constexpr const char* kAnnotationsYamlPath = "assets/graphics/annotations.yaml";

void InitNetworkSmokeRuntimeTables(Graphics& graphics) {
    const RawFrameDataFile raw_file = LoadRawFrameDataFile(kAnnotationsYamlPath);
    graphics.frame_data_db = FrameDataDb::FromRaw(raw_file);
    graphics.tile_source_db = BuildTileSourceDb(graphics.frame_data_db);

    PopulateEntityArchetypesTable();
    SyncEntityArchetypeSizesFromFrameData(graphics);
    PopulateToolArchetypesTable();
}

const Entity* FindFirstActiveEntity(const State& state) {
    for (const Entity& entity : state.entity_manager.entities) {
        if (entity.active) {
            return &entity;
        }
    }
    return nullptr;
}

const Entity* FindFirstPlayerLikeEntity(const State& state) {
    for (const Entity& entity : state.entity_manager.entities) {
        if (entity.active && IsPlayerLikeEntityType(entity.type_)) {
            return &entity;
        }
    }
    return nullptr;
}

std::optional<std::size_t> FindFirstUsableToolSlot(const State& state, VID owner_vid) {
    for (std::size_t i = 0; i < kToolSlotCount; ++i) {
        const ToolSlot* const slot = state.entity_tools.FindToolSlot(owner_vid, i);
        if (slot != nullptr && slot->active && slot->count > 0 && slot->cooldown == 0) {
            return i;
        }
    }
    return std::nullopt;
}

void ConfigureProtocolSmokeCoordinator(State& state) {
    state.net_session.role = network::NetRole::Coordinator;
    state.net_session.local_player_id = 1;
    state.net_session.coordinator_player_id = 1;
    state.net_session.stage_instance_id = 1;
    state.net_session.next_expected_coordinator_order = 1;
}

void ConfigureProtocolSmokePeer(State& state) {
    state.net_session.role = network::NetRole::Peer;
    state.net_session.local_player_id = 2;
    state.net_session.coordinator_player_id = 1;
    state.net_session.stage_instance_id = 1;
    state.net_session.next_expected_coordinator_order = 1;
}

bool ApplyCoordinatorEventsToPeer(
    State& coordinator,
    State& peer,
    const char* label,
    Audio* audio,
    Graphics* graphics
) {
    peer.net_session.ordered_events = coordinator.net_session.ordered_events;
    const std::size_t applied = network::ApplyOrderedEvents(peer.net_session, peer, audio, graphics);
    if (applied == 0 && !coordinator.net_session.ordered_events.empty()) {
        std::cerr << "network protocol smoke failed at " << label
                  << ": peer applied 0 of " << coordinator.net_session.ordered_events.size()
                  << " queued coordinator events\n";
        return false;
    }
    coordinator.net_session.ordered_events.clear();
    return true;
}

void LinkMatchingEntitiesForActionSmoke(State& coordinator, State& peer) {
    coordinator.net_session.ClearStageEntityLinks();
    peer.net_session.ClearStageEntityLinks();

    constexpr PlayerId kSmokePeerPlayerId = 2;
    network::NetEntityId next_stage_entity_id = 1000;
    const std::size_t count = std::min(
        coordinator.entity_manager.entities.size(),
        peer.entity_manager.entities.size()
    );

    for (std::size_t i = 0; i < count; ++i) {
        const Entity& coordinator_entity = coordinator.entity_manager.entities[i];
        const Entity& peer_entity = peer.entity_manager.entities[i];
        if (!coordinator_entity.active || !peer_entity.active ||
            coordinator_entity.type_ != peer_entity.type_) {
            continue;
        }

        const network::NetEntityId entity_id = IsPlayerLikeEntityType(coordinator_entity.type_)
            ? network::MakePlayerNetEntityId(kSmokePeerPlayerId)
            : next_stage_entity_id++;
        coordinator.net_session.LinkEntity(entity_id, coordinator_entity.vid);
        peer.net_session.LinkEntity(entity_id, peer_entity.vid);
        coordinator.net_session.SetEntityOwner(entity_id, std::nullopt);
        peer.net_session.SetEntityOwner(entity_id, std::nullopt);
    }
}

std::optional<VID> FindPeerEntityForCoordinatorEntity(
    const State& coordinator,
    const State& peer,
    VID coordinator_vid
) {
    const std::optional<network::NetEntityId> entity_id =
        coordinator.net_session.FindNetEntityId(coordinator_vid);
    if (!entity_id.has_value()) {
        return std::nullopt;
    }
    return peer.net_session.FindLocalVid(*entity_id);
}

bool TransferPeerActionRequestsToCoordinator(
    State& peer,
    State& coordinator,
    const char* label
) {
    bool transferred_any = false;
    for (const network::NetEvent& event : peer.net_session.pending_outbound_events) {
        if (event.type != network::NetEventType::ActionRequest) {
            std::cerr << "network action smoke failed at " << label
                      << ": peer emitted unexpected event type "
                      << static_cast<int>(event.type) << '\n';
            return false;
        }

        const network::ActionRequestEvent* const payload =
            std::get_if<network::ActionRequestEvent>(&event.payload);
        if (payload == nullptr || payload->kind == network::NetActionKind::None) {
            std::cerr << "network action smoke failed at " << label
                      << ": peer emitted malformed action request\n";
            return false;
        }

        const std::optional<VID> source_vid =
            payload->source_entity_id != network::kInvalidNetEntityId
                ? coordinator.net_session.FindLocalVid(payload->source_entity_id)
                : std::nullopt;
        const std::optional<VID> target_vid =
            payload->target_entity_id != network::kInvalidNetEntityId
                ? coordinator.net_session.FindLocalVid(payload->target_entity_id)
                : std::nullopt;
        if (payload->source_entity_id != network::kInvalidNetEntityId && !source_vid.has_value()) {
            std::cerr << "network action smoke failed at " << label
                      << ": coordinator could not resolve source net entity "
                      << payload->source_entity_id << '\n';
            return false;
        }
        if (payload->target_entity_id != network::kInvalidNetEntityId && !target_vid.has_value()) {
            std::cerr << "network action smoke failed at " << label
                      << ": coordinator could not resolve target net entity "
                      << payload->target_entity_id << '\n';
            return false;
        }

        world_ops::QueuePendingGameplayAction(
            coordinator,
            GameplayActionRequested{
                .kind = static_cast<GameplayActionKind>(payload->kind),
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
                .param_a = payload->param_a,
                .param_b = payload->param_b,
            }
        );
        transferred_any = true;
    }

    peer.net_session.pending_outbound_events.clear();
    if (!transferred_any) {
        std::cerr << "network action smoke failed at " << label
                  << ": peer emitted no action request\n";
        return false;
    }
    return true;
}

bool RunPeerActionThroughCoordinator(
    State& coordinator,
    State& peer,
    const GameplayActionRequested& peer_action,
    Graphics& graphics,
    Audio& audio,
    const char* label
) {
    world_ops::RequestGameplayAction(peer, peer_action);
    if (!TransferPeerActionRequestsToCoordinator(peer, coordinator, label)) {
        return false;
    }
    world_ops::ProcessPendingGameplayActions(coordinator, graphics, audio);
    return ApplyCoordinatorEventsToPeer(coordinator, peer, label, &audio, &graphics) &&
           CompareProtocolSmokeStates(coordinator, peer, label);
}

std::vector<network::UdpPacket> TakeCapturedPackets(network::NetTransportRuntime& transport) {
    std::vector<network::UdpPacket> packets = std::move(transport.captured_packets);
    transport.captured_packets.clear();
    return packets;
}

std::vector<network::UdpPacket> ApplyPacketDeliveryPlan(
    std::vector<network::UdpPacket> packets,
    const PacketDeliveryPlan& plan
) {
    if (plan.duplicate_each_packet) {
        std::vector<network::UdpPacket> duplicated;
        duplicated.reserve(packets.size() * 2);
        for (const network::UdpPacket& packet : packets) {
            duplicated.push_back(packet);
            duplicated.push_back(packet);
        }
        packets = std::move(duplicated);
    }
    if (plan.reverse_order) {
        std::reverse(packets.begin(), packets.end());
    }
    return packets;
}

bool DeliverPeerPacketsToCoordinator(
    State& peer,
    State& coordinator,
    network::NetTransportRuntime& peer_transport,
    network::NetTransportRuntime& coordinator_transport,
    const network::NetEndpoint& peer_endpoint,
    const char* label,
    const PacketDeliveryPlan& delivery_plan
) {
    network::SendPendingPeerEventsToCoordinator(peer, peer_transport);
    const std::vector<network::UdpPacket> packets =
        ApplyPacketDeliveryPlan(TakeCapturedPackets(peer_transport), delivery_plan);
    if (packets.empty()) {
        std::cerr << "network packet smoke failed at " << label
                  << ": peer captured no outbound packets\n";
        return false;
    }

    bool handled_any = false;
    for (const network::UdpPacket& packet : packets) {
        if (const std::optional<network::ActionRequestEventsPacket> action_requests =
                network::TryDecodeActionRequestEvents(packet.bytes.data(), packet.size)) {
            network::HandleActionRequestEventsAsCoordinator(
                coordinator,
                coordinator_transport,
                peer_endpoint,
                *action_requests
            );
            handled_any = true;
            continue;
        }
        if (const std::optional<network::PresentationCommandEventsPacket> presentation_commands =
                network::TryDecodePresentationCommandEvents(packet.bytes.data(), packet.size)) {
            network::HandlePresentationCommandEventsAsCoordinator(coordinator, *presentation_commands);
            handled_any = true;
            continue;
        }

        std::cerr << "network packet smoke failed at " << label
                  << ": coordinator could not decode peer packet of size "
                  << packet.size << '\n';
        return false;
    }

    return handled_any;
}

bool DeliverCoordinatorPacketsToPeer(
    State& coordinator,
    State& peer,
    network::NetTransportRuntime& coordinator_transport,
    network::NetTransportRuntime& peer_transport,
    const network::NetEndpoint& peer_endpoint,
    Graphics& graphics,
    Audio& audio,
    const char* label,
    bool compare_after_delivery,
    const PacketDeliveryPlan& delivery_plan
) {
    network::SendStageSyncToAllRemotes(coordinator, coordinator_transport);
    network::SendReplicatedEntityStatePatchesToAllRemotes(coordinator, coordinator_transport);
    network::SendReplicatedFluidCellPatchesToAllRemotes(coordinator, coordinator_transport);
    network::SendCoordinatorEntityRepairPatchesToAllRemotes(coordinator, coordinator_transport);
    network::SendOrderedEventsToAllRemotes(coordinator, coordinator_transport);
    const std::vector<network::UdpPacket> packets =
        ApplyPacketDeliveryPlan(TakeCapturedPackets(coordinator_transport), delivery_plan);
    if (packets.empty() && !coordinator.net_session.ordered_events.empty()) {
        std::cerr << "network packet smoke failed at " << label
                  << ": coordinator captured no result packets for "
                  << coordinator.net_session.ordered_events.size() << " ordered events\n";
        return false;
    }

    for (const network::UdpPacket& packet : packets) {
        if (const std::optional<network::StageSyncPacket> stage_sync =
                network::TryDecodeStageSync(packet.bytes.data(), packet.size)) {
            network::ApplyStageSync(peer, graphics, peer_transport, *stage_sync);
            continue;
        }
        if (const std::optional<network::TileEventsPacket> tile_events =
                network::TryDecodeTileEvents(packet.bytes.data(), packet.size)) {
            network::HandleTileEventsAsPeer(peer, *tile_events);
            continue;
        }
        if (const std::optional<network::FluidCellEventsPacket> fluid_events =
                network::TryDecodeFluidCellEvents(packet.bytes.data(), packet.size)) {
            network::HandleFluidCellEventsAsPeer(peer, *fluid_events);
            continue;
        }
        if (const std::optional<network::EntitySpawnedEventsPacket> spawn_events =
                network::TryDecodeEntitySpawnedEvents(packet.bytes.data(), packet.size)) {
            network::HandleEntitySpawnedEventsAsPeer(peer, *spawn_events);
            continue;
        }
        if (const std::optional<network::EntityDamageEventsPacket> damage_events =
                network::TryDecodeEntityDamageEvents(packet.bytes.data(), packet.size)) {
            network::HandleEntityDamageEventsAsPeer(peer, *damage_events);
            continue;
        }
        if (const std::optional<network::EntityStateEventsPacket> state_events =
                network::TryDecodeEntityStateEvents(packet.bytes.data(), packet.size)) {
            network::HandleEntityStateEventsAsPeer(peer, *state_events);
            continue;
        }
        if (const std::optional<network::EntityCarryEventsPacket> carry_events =
                network::TryDecodeEntityCarryEvents(packet.bytes.data(), packet.size)) {
            network::HandleEntityCarryEventsAsPeer(peer, *carry_events);
            continue;
        }
        if (const std::optional<network::EntityLifecycleEventsPacket> lifecycle_events =
                network::TryDecodeEntityLifecycleEvents(packet.bytes.data(), packet.size)) {
            network::HandleEntityLifecycleEventsAsPeer(peer, *lifecycle_events);
            continue;
        }
        if (const std::optional<network::PlayerStateEventsPacket> player_events =
                network::TryDecodePlayerStateEvents(packet.bytes.data(), packet.size)) {
            network::HandlePlayerStateEventsAsPeer(peer, *player_events);
            continue;
        }
        if (const std::optional<network::RunStateEventsPacket> run_events =
                network::TryDecodeRunStateEvents(packet.bytes.data(), packet.size)) {
            network::HandleRunStateEventsAsPeer(peer, *run_events);
            continue;
        }
        if (const std::optional<network::PresentationCommandEventsPacket> presentation_events =
                network::TryDecodePresentationCommandEvents(packet.bytes.data(), packet.size)) {
            network::HandlePresentationCommandEventsAsPeer(peer, *presentation_events);
            continue;
        }
        if (const std::optional<network::ActionRequestAckPacket> action_ack =
                network::TryDecodeActionRequestAck(packet.bytes.data(), packet.size)) {
            if (delivery_plan.drop_action_ack) {
                continue;
            }
            network::HandleActionRequestAckAsPeer(peer, *action_ack);
            continue;
        }

        std::cerr << "network packet smoke failed at " << label
                  << ": peer could not decode coordinator packet of size "
                  << packet.size << '\n';
        return false;
    }

    (void)network::ApplyOrderedEvents(peer.net_session, peer, &audio, &graphics);
    network::SendDurableEventAckToCoordinator(peer, peer_transport);
    for (const network::UdpPacket& packet : TakeCapturedPackets(peer_transport)) {
        if (const std::optional<network::DurableEventAckPacket> ack =
                network::TryDecodeDurableEventAck(packet.bytes.data(), packet.size)) {
            network::HandleDurableEventAckAsCoordinator(
                coordinator,
                coordinator_transport,
                peer_endpoint,
                *ack
            );
            continue;
        }
        if (const std::optional<network::ActionRequestEventsPacket> action_requests =
                network::TryDecodeActionRequestEvents(packet.bytes.data(), packet.size)) {
            network::HandleActionRequestEventsAsCoordinator(
                coordinator,
                coordinator_transport,
                peer_endpoint,
                *action_requests
            );
            continue;
        }
        std::cerr << "network packet smoke failed at " << label
                  << ": coordinator could not decode peer response packet of size "
                  << packet.size << '\n';
        return false;
    }
    (void)TakeCapturedPackets(coordinator_transport);
    return !compare_after_delivery || CompareProtocolSmokeStates(coordinator, peer, label);
}

bool DropCoordinatorPacketsToPeer(
    State& coordinator,
    network::NetTransportRuntime& coordinator_transport,
    const char* label
) {
    network::SendStageSyncToAllRemotes(coordinator, coordinator_transport);
    network::SendReplicatedEntityStatePatchesToAllRemotes(coordinator, coordinator_transport);
    network::SendReplicatedFluidCellPatchesToAllRemotes(coordinator, coordinator_transport);
    network::SendCoordinatorEntityRepairPatchesToAllRemotes(coordinator, coordinator_transport);
    network::SendOrderedEventsToAllRemotes(coordinator, coordinator_transport);
    const std::vector<network::UdpPacket> packets = TakeCapturedPackets(coordinator_transport);
    if (packets.empty()) {
        std::cerr << "network packet smoke failed at " << label
                  << ": coordinator emitted no packets to drop\n";
        return false;
    }
    return true;
}

bool RunPeerActionThroughPacketCoordinator(
    State& coordinator,
    State& peer,
    network::NetTransportRuntime& coordinator_transport,
    network::NetTransportRuntime& peer_transport,
    const network::NetEndpoint& peer_endpoint,
    const GameplayActionRequested& peer_action,
    Graphics& graphics,
    Audio& audio,
    const char* label,
    const PacketDeliveryPlan& coordinator_delivery_plan
) {
    world_ops::RequestGameplayAction(peer, peer_action);
    if (!DeliverPeerPacketsToCoordinator(
            peer,
            coordinator,
            peer_transport,
            coordinator_transport,
            peer_endpoint,
            label
        )) {
        return false;
    }
    world_ops::ProcessPendingGameplayActions(coordinator, graphics, audio);
    return DeliverCoordinatorPacketsToPeer(
        coordinator,
        peer,
        coordinator_transport,
        peer_transport,
        peer_endpoint,
        graphics,
        audio,
        label,
        true,
        coordinator_delivery_plan
    );
}

} // namespace splonks

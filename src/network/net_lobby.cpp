#include "network/net_lobby.hpp"

#include "entity/archetype.hpp"
#include "entities/common/common.hpp"
#include "graphics.hpp"
#include "network/net_entity_links.hpp"
#include "network/net_progression.hpp"
#include "network/net_protocol.hpp"
#include "quest_stage_loader.hpp"
#include "stage_spawning.hpp"
#include "state.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace splonks::network {

namespace {

constexpr PlayerId kFirstRemotePlayerId = 2;
constexpr std::uint32_t kMaxPlayersPerEndpoint = 16;
constexpr std::uint32_t kJoinRetryFrames = 30;
constexpr std::uint64_t kRemoteEndpointTimeoutFrames = 180;
constexpr float kReplicatedEntityStateMinDist = 0.01F;

NetTransportRuntime& EnsureTransport(State& state) {
    if (!state.net_transport) {
        state.net_transport = std::make_unique<NetTransportRuntime>(NetTransportRuntime::New());
    }
    return *state.net_transport;
}

Vec2 GetPrimaryPlayerSpawnPos(const State& state) {
    if (const PlayerSlot* const primary = state.players.FindPrimaryLocal()) {
        if (primary->entity_vid.has_value()) {
            if (const Entity* const entity = state.entity_manager.GetEntity(*primary->entity_vid)) {
                return entity->pos;
            }
        }
    }
    return Vec2::New(24.0F, 24.0F);
}

std::optional<Vec2> FindEntranceSpawnPos(const State& state) {
    for (unsigned int y = 0; y < state.stage.GetTileHeight(); ++y) {
        for (unsigned int x = 0; x < state.stage.GetTileWidth(); ++x) {
            if (state.stage.GetTile(x, y) == Tile::Entrance) {
                return Vec2::New(static_cast<float>(x), static_cast<float>(y)) *
                       static_cast<float>(kTileSize);
            }
        }
    }

    for (const Entity& entity : state.entity_manager.entities) {
        if (entity.active && entity.type_ == EntityType::Entrance) {
            return entity.pos;
        }
    }

    return std::nullopt;
}

Vec2 GetRemoteSpawnPos(const State& state) {
    return GetPrimaryPlayerSpawnPos(state) + Vec2::New(16.0F, 0.0F);
}

std::uint32_t MakeHostStageSeed(const State& state) {
    const std::uint32_t frame_component = state.frame == 0 ? 1U : state.frame;
    return frame_component ^ 0x51A7E5D3U;
}

bool StageCanBeNetworkSynced(const State& state) {
    return !state.stage.quest_id.empty() && !state.stage.quest_stage_id.empty();
}

bool LoadNetworkQuestStage(
    State& state,
    const std::string& quest_id,
    const std::string& quest_stage_id,
    std::uint32_t seed,
    bool preserve_player_state
) {
    return LoadQuestStage(
        state,
        quest_id,
        quest_stage_id,
        preserve_player_state,
        seed
    );
}

bool EnsureHostSyncedStage(State& state, std::string* status_out) {
    if (!StageCanBeNetworkSynced(state)) {
        if (status_out != nullptr) {
            *status_out = "Host failed: current stage is not a quest stage.";
        }
        return false;
    }

    const std::string quest_id = state.stage.quest_id;
    const std::string quest_stage_id = state.stage.quest_stage_id;
    const std::uint32_t seed = state.stage.generation_seed.value_or(MakeHostStageSeed(state));
    if (!state.stage.generation_seed.has_value()) {
        if (!LoadNetworkQuestStage(state, quest_id, quest_stage_id, seed, false)) {
            if (status_out != nullptr) {
                *status_out = "Host failed: could not reload current quest stage with sync seed.";
            }
            return false;
        }
    }

    state.net_session.quest_id = quest_id;
    state.net_session.quest_stage_id = quest_stage_id;
    state.net_session.stage_seed = seed;
    return true;
}

void RegisterRemoteEndpoint(
    NetTransportRuntime& transport,
    const std::vector<PlayerId>& player_ids,
    const NetEndpoint& endpoint,
    std::uint64_t frame
) {
    for (NetRemoteEndpoint& remote : transport.remotes) {
        if (EndpointsEqual(remote.endpoint, endpoint)) {
            remote.player_ids = player_ids;
            remote.last_heard_frame = frame;
            return;
        }
    }
    transport.remotes.push_back(NetRemoteEndpoint{
        .player_ids = player_ids,
        .endpoint = endpoint,
        .last_heard_frame = frame,
    });
}

void MarkRemoteEndpointHeard(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    std::uint64_t frame
) {
    for (NetRemoteEndpoint& remote : transport.remotes) {
        if (EndpointsEqual(remote.endpoint, endpoint)) {
            remote.last_heard_frame = frame;
            return;
        }
    }
}

void RemoveRemotePlayers(
    State& state,
    NetTransportRuntime& transport,
    const std::vector<PlayerId>& player_ids
) {
    for (PlayerId player_id : player_ids) {
        if (player_id == kInvalidPlayerId || player_id == state.net_session.local_player_id) {
            continue;
        }
        if (PlayerSlot* const slot = state.players.Find(player_id)) {
            if (slot->entity_vid.has_value()) {
                state.entity_manager.SetInactiveVid(*slot->entity_vid);
            }
        }
        state.players.Remove(player_id);
        state.net_session.UnlinkEntity(MakePlayerNetEntityId(player_id));
        state.net_session.peers.erase(
            std::remove_if(
                state.net_session.peers.begin(),
                state.net_session.peers.end(),
                [player_id](const NetPeerState& peer) { return peer.player_id == player_id; }
            ),
            state.net_session.peers.end()
        );
        transport.remote_player_targets.erase(
            std::remove_if(
                transport.remote_player_targets.begin(),
                transport.remote_player_targets.end(),
                [player_id](const NetRemotePlayerTarget& target) {
                    return target.player_id == player_id;
                }
            ),
            transport.remote_player_targets.end()
        );
    }

    for (NetRemoteEndpoint& remote : transport.remotes) {
        remote.player_ids.erase(
            std::remove_if(
                remote.player_ids.begin(),
                remote.player_ids.end(),
                [&](PlayerId remote_player_id) {
                    return std::find(player_ids.begin(), player_ids.end(), remote_player_id) !=
                           player_ids.end();
                }
            ),
            remote.player_ids.end()
        );
    }
    transport.remotes.erase(
        std::remove_if(
            transport.remotes.begin(),
            transport.remotes.end(),
            [](const NetRemoteEndpoint& remote) { return remote.player_ids.empty(); }
        ),
        transport.remotes.end()
    );
}

void RemoveRemoteEndpoint(
    State& state,
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint
) {
    std::vector<PlayerId> player_ids;
    for (const NetRemoteEndpoint& remote : transport.remotes) {
        if (EndpointsEqual(remote.endpoint, endpoint)) {
            player_ids = remote.player_ids;
            break;
        }
    }
    if (player_ids.empty()) {
        return;
    }
    RemoveRemotePlayers(state, transport, player_ids);
    transport.remotes.erase(
        std::remove_if(
            transport.remotes.begin(),
            transport.remotes.end(),
            [&](const NetRemoteEndpoint& remote) {
                return EndpointsEqual(remote.endpoint, endpoint);
            }
        ),
        transport.remotes.end()
    );
}

void CleanupTimedOutRemoteEndpoints(State& state, NetTransportRuntime& transport) {
    std::vector<NetEndpoint> timed_out;
    for (const NetRemoteEndpoint& remote : transport.remotes) {
        if (state.frame > remote.last_heard_frame &&
            state.frame - remote.last_heard_frame > kRemoteEndpointTimeoutFrames) {
            timed_out.push_back(remote.endpoint);
        }
    }
    for (const NetEndpoint& endpoint : timed_out) {
        RemoveRemoteEndpoint(state, transport, endpoint);
    }
}

std::uint32_t CountLocalPlayers(const PlayerRegistry& players) {
    std::uint32_t count = 0;
    for (const PlayerSlot& slot : players.slots) {
        if (slot.connected && slot.connection_kind == PlayerConnectionKind::Local) {
            ++count;
        }
    }
    return std::max<std::uint32_t>(count, 1);
}

std::vector<PlayerId> AllocateRemotePlayerIds(NetSessionState& session, std::uint32_t player_count) {
    std::vector<PlayerId> player_ids;
    player_ids.reserve(player_count);
    while (player_ids.size() < player_count) {
        const PlayerId player_id = std::max(session.next_player_id++, kFirstRemotePlayerId);
        const bool already_used = std::any_of(
            session.peers.begin(),
            session.peers.end(),
            [player_id](const NetPeerState& peer) { return peer.player_id == player_id; }
        );
        if (!already_used) {
            player_ids.push_back(player_id);
        }
    }
    return player_ids;
}

void EnsureSpawnedPlayer(
    State& state,
    PlayerId player_id,
    bool local,
    bool primary,
    const Vec2& pos,
    const Graphics& graphics
) {
    PlayerSlot& slot = local
        ? state.players.EnsureLocalPlayer(player_id, "Player " + std::to_string(player_id), primary)
        : state.players.EnsureRemotePlayer(player_id, "Remote " + std::to_string(player_id));

    if (slot.entity_vid.has_value()) {
        if (Entity* const entity = state.entity_manager.GetEntityMut(*slot.entity_vid)) {
            if (entity->active) {
                entity->pos = pos;
                state.net_session.LinkEntity(MakePlayerNetEntityId(player_id), entity->vid);
                if (local && primary) {
                    state.player_vid = entity->vid;
                    state.controlled_entity_vid = entity->vid;
                }
                return;
            }
        }
        slot.entity_vid.reset();
    }

    const std::optional<VID> vid = SpawnPlayerForPlayerId(state, player_id, pos);
    if (vid.has_value()) {
        state.net_session.LinkEntity(MakePlayerNetEntityId(player_id), *vid);
        state.UpdateSidForEntity(vid->id, graphics);
        if (local && primary) {
            state.player_vid = *vid;
            state.controlled_entity_vid = *vid;
        }
    }
}

void ClearLocalPlayersForJoin(State& state) {
    std::vector<PlayerId> local_player_ids;
    for (const PlayerSlot& slot : state.players.slots) {
        if (slot.connection_kind == PlayerConnectionKind::Local) {
            if (slot.entity_vid.has_value()) {
                state.entity_manager.SetInactiveVid(*slot.entity_vid);
            }
            local_player_ids.push_back(slot.player_id);
        }
    }
    for (PlayerId player_id : local_player_ids) {
        state.players.Remove(player_id);
    }
    state.debug_local_player_bots.clear();
    state.player_vid.reset();
    state.controlled_entity_vid.reset();
}

std::optional<PlayerSnapshotEntry> MakeSnapshotForSlot(const State& state, const PlayerSlot& slot) {
    if (!slot.entity_vid.has_value()) {
        return std::nullopt;
    }
    const Entity* const entity = state.entity_manager.GetEntity(*slot.entity_vid);
    if (entity == nullptr || !entity->active) {
        return std::nullopt;
    }

    PlayerSnapshotEntry snapshot;
    snapshot.player_id = slot.player_id;
    snapshot.pos_x = entity->pos.x;
    snapshot.pos_y = entity->pos.y;
    snapshot.vel_x = entity->vel.x;
    snapshot.vel_y = entity->vel.y;
    snapshot.facing = entity->facing == LeftOrRight::Right ? 1 : 0;
    snapshot.condition = static_cast<std::uint8_t>(entity->condition);
    snapshot.grounded = entity->grounded ? 1 : 0;
    snapshot.animate = entity->frame_data_animator.animate ? 1 : 0;
    snapshot.animation_id = entity->frame_data_animator.animation_id;
    snapshot.animation_frame = static_cast<std::uint16_t>(std::min<std::size_t>(
        entity->frame_data_animator.current_frame,
        std::numeric_limits<std::uint16_t>::max()
    ));
    snapshot.animation_time = entity->frame_data_animator.current_time;
    snapshot.animation_speed = entity->frame_data_animator.speed;
    return snapshot;
}

PlayerSnapshotsPacket MakeLocalPlayerSnapshots(State& state, NetTransportRuntime& transport) {
    PlayerSnapshotsPacket packet;
    packet.stage_instance_id = state.net_session.stage_instance_id;
    packet.sequence = transport.next_snapshot_sequence++;
    for (const PlayerSlot& slot : state.players.slots) {
        if (slot.connection_kind != PlayerConnectionKind::Local || !slot.connected) {
            continue;
        }
        if (packet.snapshot_count >= packet.snapshots.size()) {
            break;
        }
        const std::optional<PlayerSnapshotEntry> snapshot = MakeSnapshotForSlot(state, slot);
        if (!snapshot.has_value()) {
            continue;
        }
        packet.snapshots[packet.snapshot_count++] = *snapshot;
    }
    return packet;
}

void SendEncodedPacket(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const EncodedNetPacket& encoded
) {
    std::string error;
    if (!transport.socket.Send(endpoint, encoded.bytes.data(), encoded.size, &error)) {
        transport.last_error = error;
    }
}

void SendSnapshotsToEndpoint(
    State& state,
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint
) {
    const PlayerSnapshotsPacket snapshots = MakeLocalPlayerSnapshots(state, transport);
    if (snapshots.snapshot_count == 0) {
        return;
    }
    SendEncodedPacket(transport, endpoint, EncodePlayerSnapshots(snapshots));
}

void SendSnapshotsToAllRemotes(State& state, NetTransportRuntime& transport) {
    for (const NetRemoteEndpoint& remote : transport.remotes) {
        SendSnapshotsToEndpoint(state, transport, remote.endpoint);
    }
}

void RelaySnapshotsToOtherRemotes(
    NetTransportRuntime& transport,
    const NetEndpoint& source_endpoint,
    const PlayerSnapshotsPacket& snapshots
) {
    const EncodedNetPacket encoded = EncodePlayerSnapshots(snapshots);
    for (const NetRemoteEndpoint& remote : transport.remotes) {
        if (EndpointsEqual(remote.endpoint, source_endpoint)) {
            continue;
        }
        SendEncodedPacket(transport, remote.endpoint, encoded);
    }
}

bool IsReplicatedTileEvent(const NetEvent& event) {
    return (event.type == NetEventType::TileBroken &&
               std::holds_alternative<TileBrokenEvent>(event.payload)) ||
           (event.type == NetEventType::RopeTilePlaced &&
               std::holds_alternative<RopeTilePlacedEvent>(event.payload)) ||
           (event.type == NetEventType::TileChanged &&
               std::holds_alternative<TileChangedEvent>(event.payload));
}

bool IsReplicatedEntitySpawnedEvent(const NetEvent& event) {
    return event.type == NetEventType::EntitySpawned &&
           std::holds_alternative<EntitySpawnedEvent>(event.payload);
}

bool IsReplicatedEntityDamageEvent(const NetEvent& event) {
    return event.type == NetEventType::EntityDamaged &&
           std::holds_alternative<EntityDamagedEvent>(event.payload);
}

bool IsReplicatedEntityStateEvent(const NetEvent& event) {
    return event.type == NetEventType::EntityStatePatched &&
           std::holds_alternative<EntityStatePatchedEvent>(event.payload);
}

bool IsReplicatedEntityCarryEvent(const NetEvent& event) {
    return (event.type == NetEventType::EntityHeld &&
               std::holds_alternative<EntityHeldEvent>(event.payload)) ||
           (event.type == NetEventType::EntityDropped &&
               std::holds_alternative<EntityDroppedEvent>(event.payload)) ||
           (event.type == NetEventType::EntityThrown &&
               std::holds_alternative<EntityThrownEvent>(event.payload));
}

bool ShouldReplicateEntityStatePatch(const State& state, const Entity& entity) {
    if (!entity.active || state.players.FindPlayerIdForEntity(entity.vid).has_value()) {
        return false;
    }
    if (!state.net_session.HasLocalAuthorityForEntity(entity.vid)) {
        return false;
    }
    if (entity.dist_traveled_this_frame >= kReplicatedEntityStateMinDist) {
        return true;
    }
    if (std::abs(entity.vel.x) >= kReplicatedEntityStateMinDist ||
        std::abs(entity.vel.y) >= kReplicatedEntityStateMinDist) {
        return entity.pushable || entity.impassable || entity.crusher_pusher;
    }
    return false;
}

EntityStatePatchedEvent MakeEntityStatePayload(State& state, const Entity& entity) {
    return EntityStatePatchedEvent{
        .entity_id = GetOrAssignReplicatedEntityId(state, entity.vid),
        .source_entity_id = kInvalidNetEntityId,
        .pos = entity.pos,
        .vel = entity.vel,
        .acc = entity.acc,
        .health = entity.health,
        .stun_timer = entity.stun_timer,
        .condition = static_cast<std::uint8_t>(entity.condition),
        .grounded = static_cast<std::uint8_t>(entity.grounded ? 1 : 0),
        .active = static_cast<std::uint8_t>(entity.active ? 1 : 0),
    };
}

std::vector<NetEvent> BuildReplicatedEntityStatePatchEvents(State& state) {
    std::vector<NetEvent> events;
    for (const Entity& entity : state.entity_manager.entities) {
        if (!ShouldReplicateEntityStatePatch(state, entity)) {
            continue;
        }
        NetEvent event;
        event.header = state.net_session.MakeLocalEventHeader(state.frame);
        event.type = NetEventType::EntityStatePatched;
        event.payload = MakeEntityStatePayload(state, entity);
        events.push_back(event);
    }
    return events;
}

IVec2 GetTileEventPos(const NetEvent& event) {
    if (const TileBrokenEvent* const payload = std::get_if<TileBrokenEvent>(&event.payload)) {
        return payload->tile_pos;
    }
    if (const RopeTilePlacedEvent* const payload = std::get_if<RopeTilePlacedEvent>(&event.payload)) {
        return payload->tile_pos;
    }
    if (const TileChangedEvent* const payload = std::get_if<TileChangedEvent>(&event.payload)) {
        return payload->tile_pos;
    }
    return IVec2::New(0, 0);
}

Tile GetTileEventTile(const NetEvent& event) {
    if (const TileChangedEvent* const payload = std::get_if<TileChangedEvent>(&event.payload)) {
        return payload->tile;
    }
    if (event.type == NetEventType::RopeTilePlaced) {
        return Tile::Rope;
    }
    return Tile::Air;
}

TileEventEntry MakeTileEventEntry(const NetEvent& event) {
    const IVec2 tile_pos = GetTileEventPos(event);
    return TileEventEntry{
        .event_id = event.header.event_id,
        .source_player_id = event.header.source_player_id,
        .stage_instance_id = event.header.stage_instance_id,
        .source_local_frame = event.header.source_local_frame,
        .coordinator_order = event.header.coordinator_order,
        .event_type = static_cast<std::uint16_t>(event.type),
        .tile = static_cast<std::uint16_t>(GetTileEventTile(event)),
        .tile_x = static_cast<std::int32_t>(tile_pos.x),
        .tile_y = static_cast<std::int32_t>(tile_pos.y),
    };
}

NetEvent MakeTileEvent(const TileEventEntry& entry) {
    NetEvent event;
    event.header = NetEventHeader{
        .event_id = entry.event_id,
        .source_player_id = entry.source_player_id,
        .stage_instance_id = entry.stage_instance_id,
        .source_local_frame = entry.source_local_frame,
        .coordinator_order = entry.coordinator_order,
    };
    event.type = static_cast<NetEventType>(entry.event_type);
    const IVec2 tile_pos = IVec2::New(entry.tile_x, entry.tile_y);
    switch (event.type) {
    case NetEventType::TileBroken:
        event.payload = TileBrokenEvent{
            .tile_pos = tile_pos,
            .source_entity_id = kInvalidNetEntityId,
        };
        break;
    case NetEventType::RopeTilePlaced:
        event.payload = RopeTilePlacedEvent{
            .tile_pos = tile_pos,
            .source_entity_id = kInvalidNetEntityId,
        };
        break;
    case NetEventType::TileChanged:
        event.payload = TileChangedEvent{
            .tile_pos = tile_pos,
            .tile = static_cast<Tile>(entry.tile),
            .rotation = kTileRotation0,
        };
        break;
    default:
        event.type = NetEventType::None;
        event.payload = std::monostate{};
        break;
    }
    return event;
}

bool HasQueuedOrAppliedEvent(const NetSessionState& session, NetEventId event_id) {
    if (session.HasAppliedEvent(event_id)) {
        return true;
    }
    for (const NetEvent& event : session.pending_local_events) {
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

void SendTileEvents(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetEvent>& events
) {
    TileEventsPacket packet;
    for (const NetEvent& event : events) {
        if (!IsReplicatedTileEvent(event)) {
            continue;
        }
        if (packet.event_count >= packet.events.size()) {
            SendEncodedPacket(transport, endpoint, EncodeTileEvents(packet));
            packet = TileEventsPacket{};
        }
        packet.events[packet.event_count++] = MakeTileEventEntry(event);
    }
    if (packet.event_count > 0) {
        SendEncodedPacket(transport, endpoint, EncodeTileEvents(packet));
    }
}

EntitySpawnedEventEntry MakeEntitySpawnedEventEntry(const NetEvent& event) {
    const EntitySpawnedEvent* const payload = std::get_if<EntitySpawnedEvent>(&event.payload);
    return EntitySpawnedEventEntry{
        .event_id = event.header.event_id,
        .source_player_id = event.header.source_player_id,
        .stage_instance_id = event.header.stage_instance_id,
        .source_local_frame = event.header.source_local_frame,
        .coordinator_order = event.header.coordinator_order,
        .entity_id = payload != nullptr ? payload->entity_id : kInvalidNetEntityId,
        .held_by_id = payload != nullptr ? payload->held_by_id : kInvalidNetEntityId,
        .entity_type = payload != nullptr ? static_cast<std::uint32_t>(payload->entity_type) : 0U,
        .pos_x = payload != nullptr ? payload->pos.x : 0.0F,
        .pos_y = payload != nullptr ? payload->pos.y : 0.0F,
        .vel_x = payload != nullptr ? payload->vel.x : 0.0F,
        .vel_y = payload != nullptr ? payload->vel.y : 0.0F,
        .counter_a = payload != nullptr ? payload->counter_a : 0.0F,
        .counter_b = payload != nullptr ? payload->counter_b : 0.0F,
        .use_pressed = static_cast<std::uint8_t>(
            payload != nullptr && payload->use_pressed ? 1 : 0
        ),
    };
}

NetEvent MakeEntitySpawnedEvent(const EntitySpawnedEventEntry& entry) {
    NetEvent event;
    event.header = NetEventHeader{
        .event_id = entry.event_id,
        .source_player_id = entry.source_player_id,
        .stage_instance_id = entry.stage_instance_id,
        .source_local_frame = entry.source_local_frame,
        .coordinator_order = entry.coordinator_order,
    };
    event.type = NetEventType::EntitySpawned;
    event.payload = EntitySpawnedEvent{
        .entity_id = entry.entity_id,
        .entity_type = static_cast<EntityType>(entry.entity_type),
        .held_by_id = entry.held_by_id,
        .pos = Vec2::New(entry.pos_x, entry.pos_y),
        .vel = Vec2::New(entry.vel_x, entry.vel_y),
        .owner = NetEntityOwner::Player(entry.source_player_id),
        .counter_a = entry.counter_a,
        .counter_b = entry.counter_b,
        .use_pressed = entry.use_pressed != 0,
    };
    return event;
}

void SendEntitySpawnedEvents(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetEvent>& events
) {
    EntitySpawnedEventsPacket packet;
    for (const NetEvent& event : events) {
        if (!IsReplicatedEntitySpawnedEvent(event)) {
            continue;
        }
        if (packet.event_count >= packet.events.size()) {
            SendEncodedPacket(transport, endpoint, EncodeEntitySpawnedEvents(packet));
            packet = EntitySpawnedEventsPacket{};
        }
        packet.events[packet.event_count++] = MakeEntitySpawnedEventEntry(event);
    }
    if (packet.event_count > 0) {
        SendEncodedPacket(transport, endpoint, EncodeEntitySpawnedEvents(packet));
    }
}

EntityDamageEventEntry MakeEntityDamageEventEntry(const NetEvent& event) {
    const EntityDamagedEvent* const payload = std::get_if<EntityDamagedEvent>(&event.payload);
    return EntityDamageEventEntry{
        .event_id = event.header.event_id,
        .source_player_id = event.header.source_player_id,
        .stage_instance_id = event.header.stage_instance_id,
        .source_local_frame = event.header.source_local_frame,
        .coordinator_order = event.header.coordinator_order,
        .entity_id = payload != nullptr ? payload->entity_id : kInvalidNetEntityId,
        .source_entity_id = payload != nullptr ? payload->source_entity_id : kInvalidNetEntityId,
        .amount = payload != nullptr ? payload->amount : 0U,
        .remaining_health = payload != nullptr ? payload->remaining_health : 0U,
        .pos_x = payload != nullptr ? payload->pos.x : 0.0F,
        .pos_y = payload != nullptr ? payload->pos.y : 0.0F,
        .vel_x = payload != nullptr ? payload->vel.x : 0.0F,
        .vel_y = payload != nullptr ? payload->vel.y : 0.0F,
        .acc_x = payload != nullptr ? payload->acc.x : 0.0F,
        .acc_y = payload != nullptr ? payload->acc.y : 0.0F,
        .stun_timer = payload != nullptr ? payload->stun_timer : 0U,
        .projectile_contact_timer = payload != nullptr ? payload->projectile_contact_timer : 0U,
        .damage_type = payload != nullptr
            ? static_cast<std::uint16_t>(payload->damage_type)
            : static_cast<std::uint16_t>(0),
        .condition = payload != nullptr ? payload->condition : static_cast<std::uint8_t>(0),
        .grounded = payload != nullptr ? payload->grounded : static_cast<std::uint8_t>(0),
        .animate = payload != nullptr ? payload->animate : static_cast<std::uint8_t>(0),
        .animation_id = payload != nullptr ? payload->animation_id : kInvalidFrameDataId,
        .animation_frame = payload != nullptr ? payload->animation_frame : static_cast<std::uint16_t>(0),
        .animation_time = payload != nullptr ? payload->animation_time : 0.0F,
        .animation_speed = payload != nullptr ? payload->animation_speed : 1.0F,
    };
}

NetEvent MakeEntityDamageEvent(const EntityDamageEventEntry& entry) {
    NetEvent event;
    event.header = NetEventHeader{
        .event_id = entry.event_id,
        .source_player_id = entry.source_player_id,
        .stage_instance_id = entry.stage_instance_id,
        .source_local_frame = entry.source_local_frame,
        .coordinator_order = entry.coordinator_order,
    };
    event.type = NetEventType::EntityDamaged;
    event.payload = EntityDamagedEvent{
        .entity_id = entry.entity_id,
        .source_entity_id = entry.source_entity_id,
        .amount = entry.amount,
        .remaining_health = entry.remaining_health,
        .pos = Vec2::New(entry.pos_x, entry.pos_y),
        .vel = Vec2::New(entry.vel_x, entry.vel_y),
        .acc = Vec2::New(entry.acc_x, entry.acc_y),
        .stun_timer = entry.stun_timer,
        .projectile_contact_timer = entry.projectile_contact_timer,
        .condition = entry.condition,
        .grounded = entry.grounded,
        .animate = entry.animate,
        .animation_id = entry.animation_id,
        .animation_frame = entry.animation_frame,
        .animation_time = entry.animation_time,
        .animation_speed = entry.animation_speed,
        .damage_type = static_cast<DamageType>(entry.damage_type),
    };
    return event;
}

void SendEntityDamageEvents(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetEvent>& events
) {
    EntityDamageEventsPacket packet;
    for (const NetEvent& event : events) {
        if (!IsReplicatedEntityDamageEvent(event)) {
            continue;
        }
        if (packet.event_count >= packet.events.size()) {
            SendEncodedPacket(transport, endpoint, EncodeEntityDamageEvents(packet));
            packet = EntityDamageEventsPacket{};
        }
        packet.events[packet.event_count++] = MakeEntityDamageEventEntry(event);
    }
    if (packet.event_count > 0) {
        SendEncodedPacket(transport, endpoint, EncodeEntityDamageEvents(packet));
    }
}

EntityStateEventEntry MakeEntityStateEventEntry(const NetEvent& event) {
    const EntityStatePatchedEvent* const payload = std::get_if<EntityStatePatchedEvent>(&event.payload);
    return EntityStateEventEntry{
        .event_id = event.header.event_id,
        .source_player_id = event.header.source_player_id,
        .stage_instance_id = event.header.stage_instance_id,
        .source_local_frame = event.header.source_local_frame,
        .coordinator_order = event.header.coordinator_order,
        .entity_id = payload != nullptr ? payload->entity_id : kInvalidNetEntityId,
        .source_entity_id = payload != nullptr ? payload->source_entity_id : kInvalidNetEntityId,
        .pos_x = payload != nullptr ? payload->pos.x : 0.0F,
        .pos_y = payload != nullptr ? payload->pos.y : 0.0F,
        .vel_x = payload != nullptr ? payload->vel.x : 0.0F,
        .vel_y = payload != nullptr ? payload->vel.y : 0.0F,
        .acc_x = payload != nullptr ? payload->acc.x : 0.0F,
        .acc_y = payload != nullptr ? payload->acc.y : 0.0F,
        .health = payload != nullptr ? payload->health : 0U,
        .stun_timer = payload != nullptr ? payload->stun_timer : 0U,
        .condition = payload != nullptr ? payload->condition : static_cast<std::uint8_t>(0),
        .grounded = payload != nullptr ? payload->grounded : static_cast<std::uint8_t>(0),
        .active = payload != nullptr ? payload->active : static_cast<std::uint8_t>(0),
    };
}

NetEvent MakeEntityStateEvent(const EntityStateEventEntry& entry) {
    NetEvent event;
    event.header = NetEventHeader{
        .event_id = entry.event_id,
        .source_player_id = entry.source_player_id,
        .stage_instance_id = entry.stage_instance_id,
        .source_local_frame = entry.source_local_frame,
        .coordinator_order = entry.coordinator_order,
    };
    event.type = NetEventType::EntityStatePatched;
    event.payload = EntityStatePatchedEvent{
        .entity_id = entry.entity_id,
        .source_entity_id = entry.source_entity_id,
        .pos = Vec2::New(entry.pos_x, entry.pos_y),
        .vel = Vec2::New(entry.vel_x, entry.vel_y),
        .acc = Vec2::New(entry.acc_x, entry.acc_y),
        .health = entry.health,
        .stun_timer = entry.stun_timer,
        .condition = entry.condition,
        .grounded = entry.grounded,
        .active = entry.active,
    };
    return event;
}

void SendEntityStateEvents(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetEvent>& events
) {
    EntityStateEventsPacket packet;
    for (const NetEvent& event : events) {
        if (!IsReplicatedEntityStateEvent(event)) {
            continue;
        }
        if (packet.event_count >= packet.events.size()) {
            SendEncodedPacket(transport, endpoint, EncodeEntityStateEvents(packet));
            packet = EntityStateEventsPacket{};
        }
        packet.events[packet.event_count++] = MakeEntityStateEventEntry(event);
    }
    if (packet.event_count > 0) {
        SendEncodedPacket(transport, endpoint, EncodeEntityStateEvents(packet));
    }
}

EntityCarryEventEntry MakeEntityCarryEventEntry(const NetEvent& event) {
    EntityCarryEventEntry entry{
        .event_id = event.header.event_id,
        .source_player_id = event.header.source_player_id,
        .stage_instance_id = event.header.stage_instance_id,
        .source_local_frame = event.header.source_local_frame,
        .coordinator_order = event.header.coordinator_order,
        .event_type = static_cast<std::uint16_t>(event.type),
    };

    if (const EntityHeldEvent* const held_payload = std::get_if<EntityHeldEvent>(&event.payload)) {
        entry.entity_id = held_payload->held_id;
        entry.holder_id = held_payload->holder_id;
    } else if (const EntityDroppedEvent* const dropped_payload = std::get_if<EntityDroppedEvent>(&event.payload)) {
        entry.entity_id = dropped_payload->entity_id;
        entry.pos_x = dropped_payload->pos.x;
        entry.pos_y = dropped_payload->pos.y;
        entry.vel_x = dropped_payload->vel.x;
        entry.vel_y = dropped_payload->vel.y;
    } else if (const EntityThrownEvent* const thrown_payload = std::get_if<EntityThrownEvent>(&event.payload)) {
        entry.entity_id = thrown_payload->entity_id;
        entry.thrower_id = thrown_payload->thrower_id;
        entry.pos_x = thrown_payload->pos.x;
        entry.pos_y = thrown_payload->pos.y;
        entry.vel_x = thrown_payload->vel.x;
        entry.vel_y = thrown_payload->vel.y;
    }

    return entry;
}

NetEvent MakeEntityCarryEvent(const EntityCarryEventEntry& entry) {
    NetEvent event;
    event.header = NetEventHeader{
        .event_id = entry.event_id,
        .source_player_id = entry.source_player_id,
        .stage_instance_id = entry.stage_instance_id,
        .source_local_frame = entry.source_local_frame,
        .coordinator_order = entry.coordinator_order,
    };
    event.type = static_cast<NetEventType>(entry.event_type);
    switch (event.type) {
    case NetEventType::EntityHeld:
        event.payload = EntityHeldEvent{
            .holder_id = entry.holder_id,
            .held_id = entry.entity_id,
        };
        break;
    case NetEventType::EntityDropped:
        event.payload = EntityDroppedEvent{
            .entity_id = entry.entity_id,
            .pos = Vec2::New(entry.pos_x, entry.pos_y),
            .vel = Vec2::New(entry.vel_x, entry.vel_y),
        };
        break;
    case NetEventType::EntityThrown:
        event.payload = EntityThrownEvent{
            .entity_id = entry.entity_id,
            .pos = Vec2::New(entry.pos_x, entry.pos_y),
            .vel = Vec2::New(entry.vel_x, entry.vel_y),
            .thrower_id = entry.thrower_id,
        };
        break;
    default:
        event.type = NetEventType::None;
        event.payload = std::monostate{};
        break;
    }
    return event;
}

void SendEntityCarryEvents(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const std::vector<NetEvent>& events
) {
    EntityCarryEventsPacket packet;
    for (const NetEvent& event : events) {
        if (!IsReplicatedEntityCarryEvent(event)) {
            continue;
        }
        if (packet.event_count >= packet.events.size()) {
            SendEncodedPacket(transport, endpoint, EncodeEntityCarryEvents(packet));
            packet = EntityCarryEventsPacket{};
        }
        packet.events[packet.event_count++] = MakeEntityCarryEventEntry(event);
    }
    if (packet.event_count > 0) {
        SendEncodedPacket(transport, endpoint, EncodeEntityCarryEvents(packet));
    }
}

void SendPendingTileEventsToCoordinator(State& state, NetTransportRuntime& transport) {
    if (state.net_session.pending_local_events.empty()) {
        return;
    }
    SendTileEvents(transport, transport.coordinator_endpoint, state.net_session.pending_local_events);
    SendEntitySpawnedEvents(
        transport,
        transport.coordinator_endpoint,
        state.net_session.pending_local_events
    );
    SendEntityDamageEvents(
        transport,
        transport.coordinator_endpoint,
        state.net_session.pending_local_events
    );
    SendEntityStateEvents(
        transport,
        transport.coordinator_endpoint,
        state.net_session.pending_local_events
    );
    SendEntityCarryEvents(
        transport,
        transport.coordinator_endpoint,
        state.net_session.pending_local_events
    );
}

void SendOrderedTileEventsToAllRemotes(State& state, NetTransportRuntime& transport) {
    if (state.net_session.ordered_events.empty()) {
        return;
    }
    for (const NetRemoteEndpoint& remote : transport.remotes) {
        SendTileEvents(transport, remote.endpoint, state.net_session.ordered_events);
        SendEntitySpawnedEvents(transport, remote.endpoint, state.net_session.ordered_events);
        SendEntityDamageEvents(transport, remote.endpoint, state.net_session.ordered_events);
        SendEntityStateEvents(transport, remote.endpoint, state.net_session.ordered_events);
        SendEntityCarryEvents(transport, remote.endpoint, state.net_session.ordered_events);
    }
}

void SendReplicatedEntityStatePatchesToAllRemotes(State& state, NetTransportRuntime& transport) {
    const std::vector<NetEvent> events = BuildReplicatedEntityStatePatchEvents(state);
    if (events.empty()) {
        return;
    }
    for (const NetRemoteEndpoint& remote : transport.remotes) {
        SendEntityStateEvents(transport, remote.endpoint, events);
    }
}

void SendReplicatedEntityStatePatchesToCoordinator(State& state, NetTransportRuntime& transport) {
    const std::vector<NetEvent> events = BuildReplicatedEntityStatePatchEvents(state);
    if (events.empty()) {
        return;
    }
    SendEntityStateEvents(transport, transport.coordinator_endpoint, events);
}

void RemovePendingLocalEvent(NetSessionState& session, NetEventId event_id) {
    session.pending_local_events.erase(
        std::remove_if(
            session.pending_local_events.begin(),
            session.pending_local_events.end(),
            [event_id](const NetEvent& event) { return event.header.event_id == event_id; }
        ),
        session.pending_local_events.end()
    );
}

void HandleTileEventsAsCoordinator(State& state, const TileEventsPacket& packet) {
    for (std::uint32_t i = 0; i < packet.event_count; ++i) {
        NetEvent event = MakeTileEvent(packet.events[i]);
        if (event.header.stage_instance_id != state.net_session.stage_instance_id ||
            event.header.event_id == kInvalidNetEventId ||
            event.type == NetEventType::None ||
            HasQueuedOrAppliedEvent(state.net_session, event.header.event_id)) {
            continue;
        }
        event.header.coordinator_order = state.net_session.next_coordinator_order++;
        state.net_session.EnqueueOrderedEvent(event);
    }
}

void HandleTileEventsAsPeer(State& state, const TileEventsPacket& packet) {
    for (std::uint32_t i = 0; i < packet.event_count; ++i) {
        const TileEventEntry& entry = packet.events[i];
        if (entry.stage_instance_id != state.net_session.stage_instance_id ||
            entry.event_id == kInvalidNetEventId) {
            continue;
        }
        if (entry.source_player_id == state.net_session.local_player_id) {
            RemovePendingLocalEvent(state.net_session, entry.event_id);
            (void)state.net_session.MarkEventApplied(entry.event_id);
            continue;
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

void HandleEntitySpawnedEventsAsCoordinator(State& state, const EntitySpawnedEventsPacket& packet) {
    for (std::uint32_t i = 0; i < packet.event_count; ++i) {
        NetEvent event = MakeEntitySpawnedEvent(packet.events[i]);
        if (event.header.stage_instance_id != state.net_session.stage_instance_id ||
            event.header.event_id == kInvalidNetEventId ||
            HasQueuedOrAppliedEvent(state.net_session, event.header.event_id)) {
            continue;
        }
        event.header.coordinator_order = state.net_session.next_coordinator_order++;
        state.net_session.EnqueueOrderedEvent(event);
    }
}

void HandleEntitySpawnedEventsAsPeer(State& state, const EntitySpawnedEventsPacket& packet) {
    for (std::uint32_t i = 0; i < packet.event_count; ++i) {
        const EntitySpawnedEventEntry& entry = packet.events[i];
        if (entry.stage_instance_id != state.net_session.stage_instance_id ||
            entry.event_id == kInvalidNetEventId) {
            continue;
        }
        if (entry.source_player_id == state.net_session.local_player_id) {
            RemovePendingLocalEvent(state.net_session, entry.event_id);
            (void)state.net_session.MarkEventApplied(entry.event_id);
            continue;
        }
        if (HasQueuedOrAppliedEvent(state.net_session, entry.event_id)) {
            continue;
        }
        state.net_session.EnqueueOrderedEvent(MakeEntitySpawnedEvent(entry));
    }
}

void HandleEntityDamageEventsAsCoordinator(State& state, const EntityDamageEventsPacket& packet) {
    for (std::uint32_t i = 0; i < packet.event_count; ++i) {
        NetEvent event = MakeEntityDamageEvent(packet.events[i]);
        if (event.header.stage_instance_id != state.net_session.stage_instance_id ||
            event.header.event_id == kInvalidNetEventId ||
            HasQueuedOrAppliedEvent(state.net_session, event.header.event_id)) {
            continue;
        }
        event.header.coordinator_order = state.net_session.next_coordinator_order++;
        state.net_session.EnqueueOrderedEvent(event);
    }
}

void HandleEntityDamageEventsAsPeer(State& state, const EntityDamageEventsPacket& packet) {
    for (std::uint32_t i = 0; i < packet.event_count; ++i) {
        const EntityDamageEventEntry& entry = packet.events[i];
        if (entry.stage_instance_id != state.net_session.stage_instance_id ||
            entry.event_id == kInvalidNetEventId) {
            continue;
        }
        if (entry.source_player_id == state.net_session.local_player_id) {
            RemovePendingLocalEvent(state.net_session, entry.event_id);
            (void)state.net_session.MarkEventApplied(entry.event_id);
            continue;
        }
        if (HasQueuedOrAppliedEvent(state.net_session, entry.event_id)) {
            continue;
        }
        state.net_session.EnqueueOrderedEvent(MakeEntityDamageEvent(entry));
    }
}

void HandleEntityStateEventsAsCoordinator(State& state, const EntityStateEventsPacket& packet) {
    for (std::uint32_t i = 0; i < packet.event_count; ++i) {
        NetEvent event = MakeEntityStateEvent(packet.events[i]);
        if (event.header.stage_instance_id != state.net_session.stage_instance_id ||
            event.header.event_id == kInvalidNetEventId ||
            HasQueuedOrAppliedEvent(state.net_session, event.header.event_id)) {
            continue;
        }
        event.header.coordinator_order = state.net_session.next_coordinator_order++;
        state.net_session.EnqueueOrderedEvent(event);
    }
}

void HandleEntityStateEventsAsPeer(State& state, const EntityStateEventsPacket& packet) {
    for (std::uint32_t i = 0; i < packet.event_count; ++i) {
        const EntityStateEventEntry& entry = packet.events[i];
        if (entry.stage_instance_id != state.net_session.stage_instance_id ||
            entry.event_id == kInvalidNetEventId) {
            continue;
        }
        if (entry.source_player_id == state.net_session.local_player_id) {
            RemovePendingLocalEvent(state.net_session, entry.event_id);
            (void)state.net_session.MarkEventApplied(entry.event_id);
            continue;
        }
        if (HasQueuedOrAppliedEvent(state.net_session, entry.event_id)) {
            continue;
        }
        state.net_session.EnqueueOrderedEvent(MakeEntityStateEvent(entry));
    }
}

void HandleEntityCarryEventsAsCoordinator(State& state, const EntityCarryEventsPacket& packet) {
    for (std::uint32_t i = 0; i < packet.event_count; ++i) {
        NetEvent event = MakeEntityCarryEvent(packet.events[i]);
        if (event.header.stage_instance_id != state.net_session.stage_instance_id ||
            event.header.event_id == kInvalidNetEventId ||
            event.type == NetEventType::None ||
            HasQueuedOrAppliedEvent(state.net_session, event.header.event_id)) {
            continue;
        }
        event.header.coordinator_order = state.net_session.next_coordinator_order++;
        state.net_session.EnqueueOrderedEvent(event);
    }
}

void HandleEntityCarryEventsAsPeer(State& state, const EntityCarryEventsPacket& packet) {
    for (std::uint32_t i = 0; i < packet.event_count; ++i) {
        const EntityCarryEventEntry& entry = packet.events[i];
        if (entry.stage_instance_id != state.net_session.stage_instance_id ||
            entry.event_id == kInvalidNetEventId) {
            continue;
        }
        const bool sourced_by_local = entry.source_player_id == state.net_session.local_player_id;
        if (sourced_by_local) {
            RemovePendingLocalEvent(state.net_session, entry.event_id);
            if (static_cast<NetEventType>(entry.event_type) == NetEventType::EntityThrown) {
                (void)state.net_session.MarkEventApplied(entry.event_id);
                continue;
            }
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

NetRemotePlayerTarget& EnsureRemotePlayerTarget(
    NetTransportRuntime& transport,
    const PlayerSnapshotEntry& snapshot,
    const Vec2& current_pos,
    std::uint32_t sequence,
    std::uint64_t frame
) {
    for (NetRemotePlayerTarget& target : transport.remote_player_targets) {
        if (target.player_id == snapshot.player_id) {
            if (sequence <= target.sequence) {
                return target;
            }
            target.start_pos_x = current_pos.x;
            target.start_pos_y = current_pos.y;
            target.pos_x = snapshot.pos_x;
            target.pos_y = snapshot.pos_y;
            target.vel_x = snapshot.vel_x;
            target.vel_y = snapshot.vel_y;
            target.facing = snapshot.facing;
            target.condition = snapshot.condition;
            target.grounded = snapshot.grounded;
            target.animate = snapshot.animate;
            target.animation_id = snapshot.animation_id;
            target.animation_frame = snapshot.animation_frame;
            target.animation_time = snapshot.animation_time;
            target.animation_speed = snapshot.animation_speed;
            target.sequence = sequence;
            target.interpolation_start_frame = frame;
            target.last_received_frame = frame;
            return target;
        }
    }

    transport.remote_player_targets.push_back(NetRemotePlayerTarget{
        .player_id = snapshot.player_id,
        .start_pos_x = current_pos.x,
        .start_pos_y = current_pos.y,
        .pos_x = snapshot.pos_x,
        .pos_y = snapshot.pos_y,
        .vel_x = snapshot.vel_x,
        .vel_y = snapshot.vel_y,
        .facing = snapshot.facing,
        .condition = snapshot.condition,
        .grounded = snapshot.grounded,
        .animate = snapshot.animate,
        .animation_id = snapshot.animation_id,
        .animation_frame = snapshot.animation_frame,
        .animation_time = snapshot.animation_time,
        .animation_speed = snapshot.animation_speed,
        .sequence = sequence,
        .interpolation_start_frame = frame,
        .last_received_frame = frame,
    });
    return transport.remote_player_targets.back();
}

void ApplyPlayerSnapshots(
    State& state,
    const Graphics& graphics,
    NetTransportRuntime& transport,
    const PlayerSnapshotsPacket& packet
) {
    if (packet.stage_instance_id != state.net_session.stage_instance_id) {
        return;
    }
    for (std::uint32_t i = 0; i < packet.snapshot_count; ++i) {
        const PlayerSnapshotEntry& snapshot = packet.snapshots[i];
        if (snapshot.player_id == kInvalidPlayerId) {
            continue;
        }
        const PlayerSlot* const existing_slot = state.players.Find(snapshot.player_id);
        if (existing_slot != nullptr &&
            existing_slot->connection_kind == PlayerConnectionKind::Local) {
            continue;
        }

        Vec2 current_pos = Vec2::New(snapshot.pos_x, snapshot.pos_y);
        bool needs_spawn = true;
        if (PlayerSlot* const slot = state.players.Find(snapshot.player_id);
            slot != nullptr && slot->entity_vid.has_value()) {
            if (const Entity* const entity = state.entity_manager.GetEntity(*slot->entity_vid)) {
                if (entity->active) {
                    current_pos = entity->pos;
                    needs_spawn = false;
                }
            }
        }
        if (needs_spawn) {
            EnsureSpawnedPlayer(
                state,
                snapshot.player_id,
                false,
                false,
                Vec2::New(snapshot.pos_x, snapshot.pos_y),
                graphics
            );
        }
        (void)EnsureRemotePlayerTarget(transport, snapshot, current_pos, packet.sequence, state.frame);
    }
}

void StepRemotePlayerInterpolation(
    State& state,
    NetTransportRuntime& transport,
    const Graphics& graphics
) {
    const float strength = std::clamp(transport.remote_interpolation_strength, 0.01F, 1.0F);
    const float delay_frames = static_cast<float>(transport.remote_interpolation_delay_frames);
    const float snap_distance = std::max(0.0F, transport.remote_snap_distance);
    const float snap_distance_sq = snap_distance * snap_distance;
    for (const NetRemotePlayerTarget& target : transport.remote_player_targets) {
        PlayerSlot* const slot = state.players.Find(target.player_id);
        if (slot == nullptr ||
            slot->connection_kind == PlayerConnectionKind::Local ||
            !slot->entity_vid.has_value()) {
            continue;
        }

        Entity* const entity = state.entity_manager.GetEntityMut(*slot->entity_vid);
        if (entity == nullptr || !entity->active) {
            continue;
        }

        const bool locally_predicted_thrown =
            entity->projectile_contact_timer > 0 &&
            !entity->held_by_vid.has_value() &&
            entity->attachment_mode == AttachmentMode::None;
        if (locally_predicted_thrown) {
            state.UpdateSidForEntity(entity->vid.id, graphics);
            continue;
        }

        const bool attachment_driven =
            entity->held_by_vid.has_value() || entity->attachment_mode != AttachmentMode::None;
        const Vec2 final_target_pos = Vec2::New(target.pos_x, target.pos_y);
        if (!attachment_driven) {
            Vec2 display_target_pos = final_target_pos;
            if (delay_frames > 0.0F) {
                const float age_frames = static_cast<float>(state.frame - target.interpolation_start_frame);
                const float t = std::clamp(age_frames / delay_frames, 0.0F, 1.0F);
                const Vec2 start_pos = Vec2::New(target.start_pos_x, target.start_pos_y);
                display_target_pos = start_pos + (final_target_pos - start_pos) * t;
            }

            const Vec2 delta = display_target_pos - entity->pos;
            const float distance_sq = delta.x * delta.x + delta.y * delta.y;
            if (distance_sq > snap_distance_sq) {
                entity->pos = final_target_pos;
            } else {
                entity->pos += delta * strength;
            }
            entity->vel = Vec2::New(target.vel_x, target.vel_y);
            entity->grounded = target.grounded != 0;
        }
        entity->facing = target.facing != 0 ? LeftOrRight::Right : LeftOrRight::Left;
        entity->condition = static_cast<EntityCondition>(target.condition);
        if (target.animation_id != kInvalidFrameDataId) {
            FrameDataAnimator& animator = entity->frame_data_animator;
            if (animator.animation_id != target.animation_id) {
                animator.SetAnimation(target.animation_id);
            }
            animator.current_frame = target.animation_frame;
            animator.current_time = target.animation_time;
            animator.speed = target.animation_speed;
            animator.animate = target.animate != 0;
        }
        state.UpdateSidForEntity(entity->vid.id, graphics);
    }
}

void SyncNetworkAttachmentsAfterRemoteMovement(State& state, const Graphics& graphics) {
    constexpr int kAttachmentSyncPasses = 8;
    for (int pass = 0; pass < kAttachmentSyncPasses; ++pass) {
        for (std::size_t entity_idx = 0; entity_idx < state.entity_manager.entities.size(); ++entity_idx) {
            entities::common::SyncEntityAttachments(entity_idx, state, graphics);
        }
    }
}

bool ShouldSendSnapshots(const State& state, const NetTransportRuntime& transport) {
    const std::uint32_t interval = std::max<std::uint32_t>(transport.snapshot_send_interval_frames, 1);
    return (state.frame % interval) == 0;
}

void SendJoinRequest(State& state) {
    if (!state.net_transport || !state.net_transport->socket.IsOpen()) {
        return;
    }

    JoinRequestPacket request;
    request.local_player_count = CountLocalPlayers(state.players);
    WriteFixedString("Player", request.display_name);
    const EncodedNetPacket encoded = EncodeJoinRequest(request);
    SendEncodedPacket(*state.net_transport, state.net_transport->coordinator_endpoint, encoded);
}

LeaveNoticePacket MakeLocalLeaveNotice(const State& state) {
    LeaveNoticePacket notice;
    for (const PlayerSlot& slot : state.players.slots) {
        if (!slot.connected || slot.connection_kind != PlayerConnectionKind::Local ||
            slot.player_id == kInvalidPlayerId) {
            continue;
        }
        if (notice.player_count >= notice.player_ids.size()) {
            break;
        }
        notice.player_ids[notice.player_count++] = slot.player_id;
    }
    return notice;
}

void SendLeaveNoticeToEndpoint(NetTransportRuntime& transport, const NetEndpoint& endpoint, const State& state) {
    const LeaveNoticePacket notice = MakeLocalLeaveNotice(state);
    if (notice.player_count == 0) {
        return;
    }
    SendEncodedPacket(transport, endpoint, EncodeLeaveNotice(notice));
}

void SendLeaveNotice(State& state) {
    if (!state.net_transport || !state.net_transport->socket.IsOpen()) {
        return;
    }
    if (state.net_session.role == NetRole::Peer) {
        SendLeaveNoticeToEndpoint(*state.net_transport, state.net_transport->coordinator_endpoint, state);
        return;
    }
    if (state.net_session.role == NetRole::Coordinator) {
        for (const NetRemoteEndpoint& remote : state.net_transport->remotes) {
            SendLeaveNoticeToEndpoint(*state.net_transport, remote.endpoint, state);
        }
    }
}

std::vector<PlayerId> GetLeavePlayerIds(const LeaveNoticePacket& notice) {
    std::vector<PlayerId> player_ids;
    player_ids.reserve(notice.player_count);
    for (std::uint32_t i = 0; i < notice.player_count; ++i) {
        if (notice.player_ids[i] != kInvalidPlayerId) {
            player_ids.push_back(notice.player_ids[i]);
        }
    }
    return player_ids;
}

void HandleJoinRequest(
    State& state,
    const Graphics& graphics,
    NetTransportRuntime& transport,
    const UdpPacket& udp_packet,
    const JoinRequestPacket& request
) {
    std::uint32_t player_count = std::clamp(
        request.local_player_count,
        1U,
        kMaxPlayersPerEndpoint
    );
    std::vector<PlayerId> player_ids;
    for (const NetRemoteEndpoint& remote : transport.remotes) {
        if (EndpointsEqual(remote.endpoint, udp_packet.endpoint)) {
            player_ids = remote.player_ids;
            player_count = std::max<std::uint32_t>(
                static_cast<std::uint32_t>(player_ids.size()),
                1
            );
            break;
        }
    }
    if (player_ids.empty()) {
        player_ids = AllocateRemotePlayerIds(state.net_session, player_count);
    }

    const std::string display_name = ReadFixedString(request.display_name);
    const Vec2 remote_spawn = GetRemoteSpawnPos(state);
    for (std::size_t i = 0; i < player_ids.size(); ++i) {
        const PlayerId player_id = player_ids[i];
        const std::string player_name = display_name.empty()
            ? "Remote " + std::to_string(player_id)
            : display_name + " " + std::to_string(i + 1);
        state.players.EnsureRemotePlayer(player_id, player_name);
        EnsureSpawnedPlayer(
            state,
            player_id,
            false,
            false,
            remote_spawn + Vec2::New(static_cast<float>(i) * 8.0F, 0.0F),
            graphics
        );

        NetPeerState* peer_state = nullptr;
        for (NetPeerState& peer : state.net_session.peers) {
            if (peer.player_id == player_id) {
                peer_state = &peer;
                break;
            }
        }
        if (peer_state == nullptr) {
            NetPeerState peer;
            peer.player_id = player_id;
            state.net_session.peers.push_back(peer);
            peer_state = &state.net_session.peers.back();
        }
        peer_state->display_name = player_name;
        peer_state->endpoint_address = udp_packet.endpoint.address;
        peer_state->endpoint_port = udp_packet.endpoint.port;
    }
    RegisterRemoteEndpoint(transport, player_ids, udp_packet.endpoint, state.frame);

    const Vec2 host_spawn = GetPrimaryPlayerSpawnPos(state);
    JoinAcceptPacket accept;
    accept.assigned_player_count = static_cast<std::uint32_t>(std::min<std::size_t>(
        player_ids.size(),
        accept.assigned_player_ids.size()
    ));
    for (std::uint32_t i = 0; i < accept.assigned_player_count; ++i) {
        accept.assigned_player_ids[i] = player_ids[i];
    }
    accept.coordinator_player_id = state.net_session.coordinator_player_id;
    accept.stage_instance_id = state.net_session.stage_instance_id;
    accept.remote_spawn_x = remote_spawn.x;
    accept.remote_spawn_y = remote_spawn.y;
    accept.host_spawn_x = host_spawn.x;
    accept.host_spawn_y = host_spawn.y;
    accept.stage_seed = state.net_session.stage_seed;
    WriteFixedString(state.net_session.quest_id, accept.quest_id);
    WriteFixedString(state.net_session.quest_stage_id, accept.quest_stage_id);
    WriteFixedString("Host", accept.coordinator_name);
    const EncodedNetPacket encoded = EncodeJoinAccept(accept);
    SendEncodedPacket(transport, udp_packet.endpoint, encoded);
}

void HandleJoinAccept(
    State& state,
    const Graphics& graphics,
    NetTransportRuntime& transport,
    const JoinAcceptPacket& accept
) {
    const std::uint32_t assigned_count = std::clamp(
        accept.assigned_player_count,
        1U,
        static_cast<std::uint32_t>(accept.assigned_player_ids.size())
    );
    if (accept.assigned_player_ids[0] == kInvalidPlayerId) {
        return;
    }

    state.net_session.role = NetRole::Peer;
    state.net_session.local_player_id = accept.assigned_player_ids[0];
    state.net_session.coordinator_player_id = accept.coordinator_player_id;
    state.net_session.stage_instance_id = accept.stage_instance_id;
    state.net_session.quest_id = ReadFixedString(accept.quest_id);
    state.net_session.quest_stage_id = ReadFixedString(accept.quest_stage_id);
    state.net_session.stage_seed = accept.stage_seed;
    transport.join_request_pending = false;

    state.players = PlayerRegistry::New();
    state.player_vid.reset();
    state.controlled_entity_vid.reset();
    transport.remote_player_targets.clear();
    state.net_session.ClearStageEntityLinks();

    if (!LoadNetworkQuestStage(
            state,
            state.net_session.quest_id,
            state.net_session.quest_stage_id,
            state.net_session.stage_seed,
            false
        )) {
        transport.last_error = "Join accepted, but synced quest stage load failed.";
        return;
    }

    ClearLocalPlayersForJoin(state);
    EnsureSpawnedPlayer(
        state,
        accept.assigned_player_ids[0],
        true,
        true,
        Vec2::New(accept.remote_spawn_x, accept.remote_spawn_y),
        graphics
    );
    for (std::uint32_t i = 1; i < assigned_count; ++i) {
        const PlayerId player_id = accept.assigned_player_ids[i];
        if (player_id == kInvalidPlayerId) {
            continue;
        }
        state.players.EnsureLocalPlayer(player_id, "Player " + std::to_string(player_id), false);
        EnsureSpawnedPlayer(
            state,
            player_id,
            true,
            false,
            Vec2::New(accept.remote_spawn_x + static_cast<float>(i) * 8.0F, accept.remote_spawn_y),
            graphics
        );
    }

    state.players.EnsureRemotePlayer(accept.coordinator_player_id, ReadFixedString(accept.coordinator_name));
    NetPeerState* peer_state = nullptr;
    for (NetPeerState& peer : state.net_session.peers) {
        if (peer.player_id == accept.coordinator_player_id) {
            peer_state = &peer;
            break;
        }
    }
    if (peer_state == nullptr) {
        NetPeerState peer;
        peer.player_id = accept.coordinator_player_id;
        state.net_session.peers.push_back(peer);
        peer_state = &state.net_session.peers.back();
    }
    peer_state->display_name = ReadFixedString(accept.coordinator_name);
    peer_state->endpoint_address = transport.coordinator_endpoint.address;
    peer_state->endpoint_port = transport.coordinator_endpoint.port;
    EnsureSpawnedPlayer(
        state,
        accept.coordinator_player_id,
        false,
        false,
        Vec2::New(accept.host_spawn_x, accept.host_spawn_y),
        graphics
    );
    RegisterStageEntityLinks(state);
}

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
            HandleJoinRequest(state, graphics, transport, *packet, *request);
            continue;
        }

        if (const std::optional<LeaveNoticePacket> leave =
                TryDecodeLeaveNotice(packet->bytes.data(), packet->size)) {
            RemoveRemotePlayers(state, transport, GetLeavePlayerIds(*leave));
            continue;
        }

        if (const std::optional<StageExitRequestPacket> exit_request =
                TryDecodeStageExitRequest(packet->bytes.data(), packet->size)) {
            HandleStageExitRequestAsCoordinator(state, *exit_request);
            continue;
        }

        if (const std::optional<PlayerSnapshotsPacket> snapshots =
                TryDecodePlayerSnapshots(packet->bytes.data(), packet->size)) {
            ApplyPlayerSnapshots(state, graphics, transport, *snapshots);
            RelaySnapshotsToOtherRemotes(transport, packet->endpoint, *snapshots);
            continue;
        }

        if (const std::optional<TileEventsPacket> tile_events =
                TryDecodeTileEvents(packet->bytes.data(), packet->size)) {
            HandleTileEventsAsCoordinator(state, *tile_events);
            continue;
        }

        if (const std::optional<EntitySpawnedEventsPacket> entity_events =
                TryDecodeEntitySpawnedEvents(packet->bytes.data(), packet->size)) {
            HandleEntitySpawnedEventsAsCoordinator(state, *entity_events);
            continue;
        }

        if (const std::optional<EntityDamageEventsPacket> entity_events =
                TryDecodeEntityDamageEvents(packet->bytes.data(), packet->size)) {
            HandleEntityDamageEventsAsCoordinator(state, *entity_events);
            continue;
        }

        if (const std::optional<EntityStateEventsPacket> entity_events =
                TryDecodeEntityStateEvents(packet->bytes.data(), packet->size)) {
            HandleEntityStateEventsAsCoordinator(state, *entity_events);
            continue;
        }

        if (const std::optional<EntityCarryEventsPacket> entity_events =
                TryDecodeEntityCarryEvents(packet->bytes.data(), packet->size)) {
            HandleEntityCarryEventsAsCoordinator(state, *entity_events);
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
            RemoveRemotePlayers(state, transport, GetLeavePlayerIds(*leave));
            continue;
        }

        if (const std::optional<JoinAcceptPacket> accept =
                TryDecodeJoinAccept(packet->bytes.data(), packet->size)) {
            HandleJoinAccept(state, graphics, transport, *accept);
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
    }
}

} // namespace

bool StartHostSession(State& state, std::uint16_t port, std::string* status_out) {
    NetTransportRuntime& transport = EnsureTransport(state);
    std::string error;
    if (!transport.socket.Open(port, &error)) {
        if (status_out != nullptr) {
            *status_out = "Host failed: " + error;
        }
        return false;
    }

    state.net_session = NetSessionState::NewOffline();
    state.net_session.role = NetRole::Coordinator;
    state.net_session.local_player_id = kPrimaryLocalPlayerId;
    state.net_session.coordinator_player_id = kPrimaryLocalPlayerId;
    state.net_session.next_player_id = kFirstRemotePlayerId;
    if (!EnsureHostSyncedStage(state, status_out)) {
        transport.socket.Close();
        state.net_session = NetSessionState::NewOffline();
        return false;
    }
    state.net_session.stage_instance_id = static_cast<StageInstanceId>(state.net_session.stage_seed);
    state.players.EnsurePrimaryLocalPlayer();
    RegisterStageEntityLinks(state);
    transport.remotes.clear();
    transport.remote_player_targets.clear();
    transport.join_request_pending = false;
    if (status_out != nullptr) {
        *status_out = "Hosting UDP on port " + std::to_string(transport.socket.BoundPort()) + ".";
    }
    return true;
}

bool JoinHostSession(
    State& state,
    const std::string& host,
    std::uint16_t port,
    std::string* status_out
) {
    NetTransportRuntime& transport = EnsureTransport(state);
    std::string error;
    if (!transport.socket.Open(0, &error)) {
        if (status_out != nullptr) {
            *status_out = "Join failed: " + error;
        }
        return false;
    }

    state.net_session = NetSessionState::NewOffline();
    state.net_session.role = NetRole::Peer;
    state.net_session.local_player_id = kPrimaryLocalPlayerId;
    state.net_session.coordinator_player_id = kPrimaryLocalPlayerId;
    transport.coordinator_endpoint = NetEndpoint{.address = host, .port = port};
    transport.remotes.clear();
    transport.remote_player_targets.clear();
    transport.join_request_pending = true;
    transport.join_request_retry_frames = 0;
    SendJoinRequest(state);
    if (status_out != nullptr) {
        *status_out = "Joining " + EndpointToString(transport.coordinator_endpoint) + ".";
    }
    return true;
}

void DisconnectSession(State& state, std::string* status_out) {
    if (state.net_transport) {
        SendLeaveNotice(state);
        state.net_transport->socket.Close();
        state.net_transport->remotes.clear();
        state.net_transport->remote_player_targets.clear();
        state.net_transport->join_request_pending = false;
    }
    state.net_session = NetSessionState::NewOffline();
    if (status_out != nullptr) {
        *status_out = "Disconnected.";
    }
}

bool RespawnLocalPlayersAtEntrance(State& state, const Graphics& graphics, std::string* status_out) {
    if (state.net_session.role == NetRole::Offline) {
        if (status_out != nullptr) {
            *status_out = "No network session is active.";
        }
        return false;
    }

    const std::optional<Vec2> entrance_pos = FindEntranceSpawnPos(state);
    if (!entrance_pos.has_value()) {
        if (status_out != nullptr) {
            *status_out = "Network respawn failed: no entrance was found.";
        }
        return false;
    }

    int local_index = 0;
    for (PlayerSlot& slot : state.players.slots) {
        if (!slot.connected || slot.connection_kind != PlayerConnectionKind::Local) {
            continue;
        }

        const Vec2 spawn_pos = *entrance_pos + Vec2::New(static_cast<float>(local_index) * 8.0F, 0.0F);
        ++local_index;

        Entity* entity = nullptr;
        if (slot.entity_vid.has_value()) {
            entity = state.entity_manager.GetEntityMut(*slot.entity_vid);
        }
        if (entity == nullptr) {
            EnsureSpawnedPlayer(
                state,
                slot.player_id,
                true,
                slot.primary_local,
                spawn_pos,
                graphics
            );
            continue;
        }

        const EntityType respawn_type =
            IsPlayerLikeEntityType(entity->type_) ? entity->type_ : EntityType::Player;
        SetEntityAs(*entity, respawn_type);
        entity->pos = spawn_pos;
        entity->vel = Vec2::New(0.0F, 0.0F);
        entity->acc = Vec2::New(0.0F, 0.0F);
        entity->grounded = false;
        entity->coyote_time = 0;
        entity->fall_timer = 0;
        entity->stun_timer = 0;
        entity->render_enabled = GetEntityArchetype(entity->type_).render_enabled;
        state.UpdateSidForEntity(entity->vid.id, graphics);
        if (slot.primary_local) {
            state.player_vid = entity->vid;
            state.controlled_entity_vid = entity->vid;
        }
    }

    for (Entity& entity : state.entity_manager.entities) {
        if (!entity.active || entity.type_ != EntityType::Entrance) {
            continue;
        }
        entity.counter_a = 0.0F;
        entity.counter_b = 0.0F;
    }

    state.game_over = false;
    state.pending_stage_transition.reset();
    state.gameplay_camera_anchor_world_pos.reset();
    if (status_out != nullptr) {
        *status_out = "Respawned local network players at entrance.";
    }
    return true;
}

bool ReloadSyncedQuestStage(State& state, const Graphics& graphics, std::string* status_out) {
    if (state.net_session.role == NetRole::Offline) {
        if (status_out != nullptr) {
            *status_out = "No synced network stage is active.";
        }
        return false;
    }
    if (state.net_session.quest_id.empty() || state.net_session.quest_stage_id.empty()) {
        if (status_out != nullptr) {
            *status_out = "No synced quest stage metadata is available.";
        }
        return false;
    }
    struct SavedPlayerSlot {
        PlayerId player_id = kInvalidPlayerId;
        bool local = false;
        bool primary = false;
    };
    std::vector<SavedPlayerSlot> saved_slots;
    saved_slots.reserve(state.players.slots.size());
    for (const PlayerSlot& slot : state.players.slots) {
        if (!slot.connected || slot.player_id == kInvalidPlayerId) {
            continue;
        }
        saved_slots.push_back(SavedPlayerSlot{
            .player_id = slot.player_id,
            .local = slot.connection_kind == PlayerConnectionKind::Local,
            .primary = slot.primary_local,
        });
    }

    state.players = PlayerRegistry::New();
    if (state.net_transport) {
        state.net_transport->remote_player_targets.clear();
    }
    const bool loaded = LoadNetworkQuestStage(
        state,
        state.net_session.quest_id,
        state.net_session.quest_stage_id,
        state.net_session.stage_seed,
        false
    );
    if (!loaded) {
        if (status_out != nullptr) {
            *status_out = "Synced stage reload failed.";
        }
        return false;
    }
    RegisterStageEntityLinks(state);
    const Vec2 spawn_base = GetPrimaryPlayerSpawnPos(state);
    if (state.player_vid.has_value()) {
        state.entity_manager.SetInactiveVid(*state.player_vid);
    }
    state.players.slots.clear();
    state.player_vid.reset();
    state.controlled_entity_vid.reset();
    for (std::size_t i = 0; i < saved_slots.size(); ++i) {
        const SavedPlayerSlot& slot = saved_slots[i];
        EnsureSpawnedPlayer(
            state,
            slot.player_id,
            slot.local,
            slot.primary,
            spawn_base + Vec2::New(static_cast<float>(i) * 8.0F, 0.0F),
            graphics
        );
    }

    if (status_out != nullptr) {
        *status_out = "Reloaded synced stage " + state.net_session.quest_stage_id +
                      " seed " + std::to_string(state.net_session.stage_seed) + ".";
    }
    return true;
}

void StepNetworkLobby(State& state, const Graphics& graphics) {
    if (!state.net_transport || !state.net_transport->socket.IsOpen()) {
        return;
    }
    if (state.net_session.role == NetRole::Coordinator) {
        StepHostPackets(state, graphics, *state.net_transport);
        const bool should_send = ShouldSendSnapshots(state, *state.net_transport);
        if (should_send) {
            SendStageSyncToAllRemotes(state, *state.net_transport);
            SendSnapshotsToAllRemotes(state, *state.net_transport);
            SendReplicatedEntityStatePatchesToAllRemotes(state, *state.net_transport);
            SendOrderedTileEventsToAllRemotes(state, *state.net_transport);
        }
    } else if (state.net_session.role == NetRole::Peer) {
        StepPeerPackets(state, graphics, *state.net_transport);
        const bool should_send = ShouldSendSnapshots(state, *state.net_transport);
        if (!state.net_transport->join_request_pending && should_send) {
            SendSnapshotsToEndpoint(
                state,
                *state.net_transport,
                state.net_transport->coordinator_endpoint
            );
            SendReplicatedEntityStatePatchesToCoordinator(state, *state.net_transport);
            SendPendingTileEventsToCoordinator(state, *state.net_transport);
        }
    }
    StepRemotePlayerInterpolation(state, *state.net_transport, graphics);
    SyncNetworkAttachmentsAfterRemoteMovement(state, graphics);
}

bool IsTransportOpen(const State& state) {
    return state.net_transport && state.net_transport->socket.IsOpen();
}

std::uint16_t BoundTransportPort(const State& state) {
    if (!IsTransportOpen(state)) {
        return 0;
    }
    return state.net_transport->socket.BoundPort();
}

} // namespace splonks::network

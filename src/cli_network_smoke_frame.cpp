#include "cli_network_smoke.hpp"

#include "cli_network_smoke_internal.hpp"
#include "buying.hpp"
#include "network/net_lobby_internal.hpp"
#include "network/net_lobby.hpp"
#include "quest_stage_loader.hpp"
#include "step.hpp"
#include "tools/tool_archetype.hpp"
#include "world_ops.hpp"

#include <cmath>
#include <exception>
#include <iostream>
#include <memory>
#include <optional>

namespace splonks {

namespace {

struct PacketSmokePair {
    network::NetEndpoint coordinator_endpoint{
        .address = "127.0.0.1",
        .port = 43201,
    };
    network::NetEndpoint peer_endpoint{
        .address = "127.0.0.1",
        .port = 43202,
    };
};

PacketSmokePair MakePacketSmokePair() {
    PacketSmokePair pair;
    return pair;
}

std::vector<network::UdpPacket> TakeCapturedSnapshotPackets(network::NetTransportRuntime& transport) {
    std::vector<network::UdpPacket> packets = std::move(transport.captured_packets);
    transport.captured_packets.clear();
    return packets;
}

void AttachPacketSmokeTransports(State& coordinator, State& peer, const PacketSmokePair& pair) {
    coordinator.net_transport =
        std::make_unique<network::NetTransportRuntime>(network::NetTransportRuntime::New());
    peer.net_transport =
        std::make_unique<network::NetTransportRuntime>(network::NetTransportRuntime::New());

    coordinator.net_transport->capture_outgoing_packets = true;
    peer.net_transport->capture_outgoing_packets = true;
    peer.net_transport->coordinator_endpoint = pair.coordinator_endpoint;
    coordinator.net_transport->remotes.push_back(network::NetRemoteEndpoint{
        .player_ids = {2},
        .endpoint = pair.peer_endpoint,
        .last_heard_frame = 0,
        .highest_acked_coordinator_order = 0,
    });
}

network::NetTransportRuntime& CoordinatorSmokeTransport(State& coordinator) {
    return *coordinator.net_transport;
}

network::NetTransportRuntime& PeerSmokeTransport(State& peer) {
    return *peer.net_transport;
}

bool StepPeerActionThroughCoordinatorFrame(
    State& coordinator,
    State& peer,
    PacketSmokePair& pair,
    const GameplayActionRequested& peer_action,
    Graphics& graphics,
    Audio& audio,
    const char* label,
    bool compare_after_delivery = true
) {
    world_ops::QueuePendingGameplayAction(peer, peer_action);
    StepSingleTick(peer, audio, graphics);
    if (!DeliverPeerPacketsToCoordinator(
            peer,
            coordinator,
            PeerSmokeTransport(peer),
            CoordinatorSmokeTransport(coordinator),
            pair.peer_endpoint,
            label
        )) {
        return false;
    }

    StepSingleTick(coordinator, audio, graphics);
    return DeliverCoordinatorPacketsToPeer(
        coordinator,
        peer,
        CoordinatorSmokeTransport(coordinator),
        PeerSmokeTransport(peer),
        pair.peer_endpoint,
        graphics,
        audio,
        label,
        compare_after_delivery
    );
}

bool StepIdleFrameAndCompare(
    State& coordinator,
    State& peer,
    PacketSmokePair& pair,
    Graphics& graphics,
    Audio& audio,
    const char* label
) {
    StepSingleTick(peer, audio, graphics);
    if (!peer.net_session.pending_outbound_events.empty() &&
        !DeliverPeerPacketsToCoordinator(
            peer,
            coordinator,
            PeerSmokeTransport(peer),
            CoordinatorSmokeTransport(coordinator),
            pair.peer_endpoint,
            label
        )) {
        return false;
    }

    StepSingleTick(coordinator, audio, graphics);
    return DeliverCoordinatorPacketsToPeer(
        coordinator,
        peer,
        CoordinatorSmokeTransport(coordinator),
        PeerSmokeTransport(peer),
        pair.peer_endpoint,
        graphics,
        audio,
        label
    );
}

bool LinkAndCompareInitialStates(State& coordinator, State& peer) {
    LinkMatchingEntitiesForActionSmoke(coordinator, peer);
    return CompareProtocolSmokeStates(coordinator, peer, "frame after load");
}

void SetMovementSmokeInput(State& state, bool right, bool run, bool jump) {
    state.playing_input_snapshot = PlayingInputSnapshot::New();
    state.playing_input_snapshot.right = right;
    state.playing_input_snapshot.run = run;
    state.playing_input_snapshot.jump = jump;
}

bool DeliverPeerSnapshotsToCoordinator(
    State& peer,
    State& coordinator,
    PacketSmokePair& pair,
    const Graphics& graphics,
    const char* label
) {
    network::SendSnapshotsToEndpoint(peer, PeerSmokeTransport(peer), pair.coordinator_endpoint);
    const std::vector<network::UdpPacket> packets =
        TakeCapturedSnapshotPackets(PeerSmokeTransport(peer));
    if (packets.empty()) {
        std::cerr << "network frame smoke failed at " << label
                  << ": peer emitted no player snapshot packet\n";
        return false;
    }

    bool handled_snapshot = false;
    for (const network::UdpPacket& packet : packets) {
        if (const std::optional<network::PlayerSnapshotsPacket> snapshots =
                network::TryDecodePlayerSnapshots(packet.bytes.data(), packet.size)) {
            network::ApplyPlayerSnapshots(
                coordinator,
                graphics,
                CoordinatorSmokeTransport(coordinator),
                *snapshots
            );
            handled_snapshot = true;
            continue;
        }
    }
    if (!handled_snapshot) {
        std::cerr << "network frame smoke failed at " << label
                  << ": coordinator could not decode peer snapshot packet\n";
    }
    return handled_snapshot;
}

bool DeliverCoordinatorSnapshotsToPeer(
    State& coordinator,
    State& peer,
    PacketSmokePair& pair,
    const Graphics& graphics,
    const char* label
) {
    network::SendSnapshotsToEndpoint(
        coordinator,
        CoordinatorSmokeTransport(coordinator),
        pair.peer_endpoint
    );
    const std::vector<network::UdpPacket> packets =
        TakeCapturedSnapshotPackets(CoordinatorSmokeTransport(coordinator));
    if (packets.empty()) {
        std::cerr << "network frame smoke failed at " << label
                  << ": coordinator emitted no player snapshot packet\n";
        return false;
    }

    bool handled_snapshot = false;
    for (const network::UdpPacket& packet : packets) {
        if (const std::optional<network::PlayerSnapshotsPacket> snapshots =
                network::TryDecodePlayerSnapshots(packet.bytes.data(), packet.size)) {
            network::ApplyPlayerSnapshots(
                peer,
                graphics,
                PeerSmokeTransport(peer),
                *snapshots
            );
            handled_snapshot = true;
            continue;
        }
    }
    if (!handled_snapshot) {
        std::cerr << "network frame smoke failed at " << label
                  << ": peer could not decode coordinator snapshot packet\n";
    }
    return handled_snapshot;
}

bool ConfigureMovementSmokePlayer(
    State& coordinator,
    State& peer,
    const Graphics& graphics,
    VID& coordinator_player_vid,
    VID& peer_player_vid
) {
    Entity* coordinator_player = nullptr;
    Entity* peer_player = nullptr;
    if (const Entity* const source = FindFirstPlayerLikeEntity(coordinator)) {
        coordinator_player = coordinator.entity_manager.GetEntityMut(source->vid);
    }
    if (const Entity* const source = FindFirstPlayerLikeEntity(peer)) {
        peer_player = peer.entity_manager.GetEntityMut(source->vid);
    }
    if (coordinator_player == nullptr || peer_player == nullptr) {
        return false;
    }

    constexpr PlayerId kPeerPlayerId = 2;
    const network::NetEntityId peer_player_net_id = network::MakePlayerNetEntityId(kPeerPlayerId);
    coordinator.players.slots.clear();
    peer.players.slots.clear();
    PlayerSlot& coordinator_slot =
        coordinator.players.EnsureRemotePlayer(kPeerPlayerId, "Remote 2");
    PlayerSlot& peer_slot =
        peer.players.EnsureLocalPlayer(kPeerPlayerId, "Player 2", true);
    coordinator_slot.entity_vid = coordinator_player->vid;
    peer_slot.entity_vid = peer_player->vid;
    peer.controlled_entity_vid = peer_player->vid;
    coordinator.net_session.LinkEntity(peer_player_net_id, coordinator_player->vid);
    peer.net_session.LinkEntity(peer_player_net_id, peer_player->vid);
    coordinator.net_session.SetEntityOwner(peer_player_net_id, kPeerPlayerId);
    peer.net_session.SetEntityOwner(peer_player_net_id, kPeerPlayerId);

    for (State* const state : {&coordinator, &peer}) {
        for (int x = 2; x <= 8; ++x) {
            state->stage.SetTile(IVec2::New(x, 8), Tile::CaveBlock);
            state->stage.SetTile(IVec2::New(x, 7), Tile::Air);
            state->stage.SetTile(IVec2::New(x, 6), Tile::Air);
        }
    }

    for (Entity* const entity : {coordinator_player, peer_player}) {
        entity->pos = Vec2::New(64.0F, 114.0F);
        entity->vel = Vec2::New(0.0F, 0.0F);
        entity->acc = Vec2::New(0.0F, 0.0F);
        entity->grounded = true;
        entity->coyote_time = 3;
        entity->fall_timer = 0;
        entity->stun_timer = 0;
        entity->condition = EntityCondition::Normal;
    }
    coordinator.UpdateSidForEntity(coordinator_player->vid.id, graphics);
    peer.UpdateSidForEntity(peer_player->vid.id, graphics);
    coordinator_player_vid = coordinator_player->vid;
    peer_player_vid = peer_player->vid;
    return true;
}

bool RunBasicRemoteMovementSmoke(Graphics& graphics, Audio& audio) {
    constexpr std::uint32_t seed = 12345;
    State coordinator = State::New();
    State peer = State::New();
    ConfigureProtocolSmokeCoordinator(coordinator);
    ConfigureProtocolSmokePeer(peer);

    if (!LoadQuestStage(coordinator, "classic", "classic_mines_1", false, seed) ||
        !LoadQuestStage(peer, "classic", "classic_mines_1", false, seed)) {
        std::cerr << "network frame smoke failed: could not load movement test stages\n";
        return false;
    }
    ConfigureProtocolSmokeCoordinator(coordinator);
    ConfigureProtocolSmokePeer(peer);
    PacketSmokePair pair = MakePacketSmokePair();
    AttachPacketSmokeTransports(coordinator, peer, pair);

    VID coordinator_player_vid;
    VID peer_player_vid;
    if (!ConfigureMovementSmokePlayer(
            coordinator,
            peer,
            graphics,
            coordinator_player_vid,
            peer_player_vid
        )) {
        std::cerr << "network frame smoke failed: could not configure movement player\n";
        return false;
    }

    constexpr int kFrames = 30;
    for (int frame = 0; frame < kFrames; ++frame) {
        const bool jump = frame >= 1 && frame <= 5;
        SetMovementSmokeInput(peer, true, true, jump);
        StepSingleTick(peer, audio, graphics);
        if (!DeliverPeerSnapshotsToCoordinator(
                peer,
                coordinator,
                pair,
                graphics,
                "frame basic remote movement snapshot"
            )) {
            return false;
        }
        StepSingleTick(coordinator, audio, graphics);
    }

    const Entity* const coordinator_player =
        coordinator.entity_manager.GetEntity(coordinator_player_vid);
    const Entity* const peer_player = peer.entity_manager.GetEntity(peer_player_vid);
    if (coordinator_player == nullptr || peer_player == nullptr) {
        std::cerr << "network frame smoke failed: missing movement player after sim\n";
        return false;
    }

    const Vec2 delta = coordinator_player->pos - peer_player->pos;
    const float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    constexpr float kMaxMovementDesyncPx = 2.0F;
    if (distance > kMaxMovementDesyncPx) {
        std::cerr << "network frame smoke failed at frame basic remote movement:"
                  << " coordinator pos=" << coordinator_player->pos.x << ","
                  << coordinator_player->pos.y
                  << " peer pos=" << peer_player->pos.x << ","
                  << peer_player->pos.y
                  << " distance=" << distance << '\n';
        return false;
    }

    Entity* const peer_player_mut = peer.entity_manager.GetEntityMut(peer_player_vid);
    if (peer_player_mut == nullptr) {
        std::cerr << "network frame smoke failed: missing mutable peer movement player\n";
        return false;
    }
    peer_player_mut->pos += Vec2::New(-80.0F, 0.0F);
    peer_player_mut->health = 123;
    peer_player_mut->fall_timer = 99;
    peer_player_mut->stun_timer = 77;
    peer_player_mut->projectile_contact_timer = 55;
    peer_player_mut->condition = EntityCondition::Stunned;
    peer_player_mut->grounded = false;
    peer.UpdateSidForEntity(peer_player_vid.id, graphics);
    if (!DeliverCoordinatorSnapshotsToPeer(
            coordinator,
            peer,
            pair,
            graphics,
            "frame basic local player repair snapshot"
        )) {
        return false;
    }
    network::StepRemotePlayerInterpolation(peer, PeerSmokeTransport(peer), graphics);
    const Entity* const repaired_peer_player = peer.entity_manager.GetEntity(peer_player_vid);
    if (repaired_peer_player == nullptr) {
        std::cerr << "network frame smoke failed: missing repaired peer player\n";
        return false;
    }
    const Vec2 repair_delta = coordinator_player->pos - repaired_peer_player->pos;
    const float repair_distance =
        std::sqrt(repair_delta.x * repair_delta.x + repair_delta.y * repair_delta.y);
    if (repair_distance > kMaxMovementDesyncPx) {
        std::cerr << "network frame smoke failed at frame basic local player repair:"
                  << " coordinator pos=" << coordinator_player->pos.x << ","
                  << coordinator_player->pos.y
                  << " peer pos=" << repaired_peer_player->pos.x << ","
                  << repaired_peer_player->pos.y
                  << " distance=" << repair_distance << '\n';
        return false;
    }
    if (repaired_peer_player->health != coordinator_player->health ||
        repaired_peer_player->fall_timer != coordinator_player->fall_timer ||
        repaired_peer_player->stun_timer != coordinator_player->stun_timer ||
        repaired_peer_player->projectile_contact_timer != coordinator_player->projectile_contact_timer ||
        repaired_peer_player->condition != coordinator_player->condition ||
        repaired_peer_player->grounded != coordinator_player->grounded) {
        std::cerr << "network frame smoke failed at frame basic local player state repair:"
                  << " coordinator health=" << coordinator_player->health
                  << " fall=" << coordinator_player->fall_timer
                  << " stun=" << coordinator_player->stun_timer
                  << " condition=" << static_cast<int>(coordinator_player->condition)
                  << " grounded=" << coordinator_player->grounded
                  << " peer health=" << repaired_peer_player->health
                  << " fall=" << repaired_peer_player->fall_timer
                  << " stun=" << repaired_peer_player->stun_timer
                  << " condition=" << static_cast<int>(repaired_peer_player->condition)
                  << " grounded=" << repaired_peer_player->grounded << '\n';
        return false;
    }

    std::cout << "network frame smoke basic remote movement ok: distance="
              << distance << " repair_distance=" << repair_distance << '\n';
    return true;
}

std::optional<VID> SpawnCoordinatorEntityAndResolvePeer(
    State& coordinator,
    State& peer,
    PacketSmokePair& pair,
    Graphics& graphics,
    Audio& audio,
    EntityType type_,
    Vec2 pos,
    const char* label
) {
    Entity* const spawned = world_ops::SpawnEntity(
        coordinator,
        type_,
        [pos](Entity& entity) {
            entity.pos = pos;
            entity.vel = Vec2::New(0.0F, 0.0F);
            entity.acc = Vec2::New(0.0F, 0.0F);
        }
    );
    if (spawned == nullptr) {
        std::cerr << "network frame smoke failed at " << label
                  << ": coordinator could not spawn entity type "
                  << static_cast<int>(type_) << '\n';
        return std::nullopt;
    }
    const VID coordinator_vid = spawned->vid;
    if (!DeliverCoordinatorPacketsToPeer(
            coordinator,
            peer,
            CoordinatorSmokeTransport(coordinator),
            PeerSmokeTransport(peer),
            pair.peer_endpoint,
            graphics,
            audio,
            label
        )) {
        return std::nullopt;
    }
    const std::optional<VID> peer_vid =
        FindPeerEntityForCoordinatorEntity(coordinator, peer, coordinator_vid);
    if (!peer_vid.has_value()) {
        std::cerr << "network frame smoke failed at " << label
                  << ": peer could not resolve spawned entity\n";
    }
    return peer_vid;
}

const Entity* FindFirstEntityOfType(const State& state, EntityType type_) {
    for (const Entity& entity : state.entity_manager.entities) {
        if (entity.active && entity.type_ == type_) {
            return &entity;
        }
    }
    return nullptr;
}

bool MovePeerAndCoordinatorEntityTo(
    State& coordinator,
    State& peer,
    VID peer_vid,
    Vec2 pos
) {
    Entity* const peer_entity = peer.entity_manager.GetEntityMut(peer_vid);
    if (peer_entity == nullptr) {
        return false;
    }
    const std::optional<network::NetEntityId> entity_id =
        peer.net_session.FindNetEntityId(peer_vid);
    if (!entity_id.has_value()) {
        return false;
    }
    const std::optional<VID> coordinator_vid =
        coordinator.net_session.FindLocalVid(*entity_id);
    if (!coordinator_vid.has_value()) {
        return false;
    }
    Entity* const coordinator_entity = coordinator.entity_manager.GetEntityMut(*coordinator_vid);
    if (coordinator_entity == nullptr) {
        return false;
    }

    for (Entity* const entity : {coordinator_entity, peer_entity}) {
        entity->pos = pos;
        entity->vel = Vec2::New(0.0F, 0.0F);
        entity->acc = Vec2::New(0.0F, 0.0F);
        entity->grounded = true;
        entity->fall_timer = 0;
    }
    return true;
}

std::optional<VID> ResolveCoordinatorVidForPeerVid(
    const State& coordinator,
    const State& peer,
    VID peer_vid
) {
    const std::optional<network::NetEntityId> entity_id = peer.net_session.FindNetEntityId(peer_vid);
    if (!entity_id.has_value()) {
        return std::nullopt;
    }
    return coordinator.net_session.FindLocalVid(*entity_id);
}

bool PickupSpawnedEntityForFrameSmoke(
    State& coordinator,
    State& peer,
    PacketSmokePair& pair,
    VID peer_source_vid,
    VID peer_item_vid,
    Graphics& graphics,
    Audio& audio,
    const char* label
) {
    return StepPeerActionThroughCoordinatorFrame(
        coordinator,
        peer,
        pair,
        GameplayActionRequested{
            .kind = GameplayActionKind::PickupEntity,
            .source_vid = peer_source_vid,
            .target_vid = peer_item_vid,
        },
        graphics,
        audio,
        label
    );
}

bool RunHeldUseScenario(
    State& coordinator,
    State& peer,
    PacketSmokePair& pair,
    VID peer_source_vid,
    Vec2 peer_source_pos,
    EntityType item_type,
    const char* label,
    bool release_after_press,
    Graphics& graphics,
    Audio& audio
) {
    const std::optional<VID> peer_item_vid = SpawnCoordinatorEntityAndResolvePeer(
        coordinator,
        peer,
        pair,
        graphics,
        audio,
        item_type,
        peer_source_pos + Vec2::New(8.0F, 0.0F),
        label
    );
    if (!peer_item_vid.has_value() ||
        !PickupSpawnedEntityForFrameSmoke(
            coordinator,
            peer,
            pair,
            peer_source_vid,
            *peer_item_vid,
            graphics,
            audio,
            label
        )) {
        return false;
    }

    if (!StepPeerActionThroughCoordinatorFrame(
            coordinator,
            peer,
            pair,
            GameplayActionRequested{
                .kind = GameplayActionKind::UseHeldEntity,
                .source_vid = peer_source_vid,
                .target_vid = *peer_item_vid,
                .direction = IVec2::New(1, 0),
                .param_a = 1,
            },
            graphics,
            audio,
            label
        ) ||
        !StepIdleFrameAndCompare(coordinator, peer, pair, graphics, audio, label)) {
        return false;
    }

    if (release_after_press) {
        if (!StepPeerActionThroughCoordinatorFrame(
                coordinator,
                peer,
                pair,
                GameplayActionRequested{
                    .kind = GameplayActionKind::UseHeldEntity,
                    .source_vid = peer_source_vid,
                    .target_vid = *peer_item_vid,
                    .direction = IVec2::New(1, 0),
                    .param_a = 0,
                },
                graphics,
                audio,
                label
            ) ||
            !StepIdleFrameAndCompare(coordinator, peer, pair, graphics, audio, label)) {
            return false;
        }
    }
    return StepPeerActionThroughCoordinatorFrame(
        coordinator,
        peer,
        pair,
        GameplayActionRequested{
            .kind = GameplayActionKind::DropEntity,
            .source_vid = peer_source_vid,
            .target_vid = *peer_item_vid,
        },
        graphics,
        audio,
        label
    );
}

bool RunBackUseScenario(
    State& coordinator,
    State& peer,
    PacketSmokePair& pair,
    VID peer_source_vid,
    Vec2 peer_source_pos,
    EntityType item_type,
    const char* label,
    bool expect_peer_sprite_particles,
    Graphics& graphics,
    Audio& audio
) {
    if (const Entity* const source = peer.entity_manager.GetEntity(peer_source_vid);
        source != nullptr && source->back_vid.has_value()) {
        const VID peer_existing_back_vid = *source->back_vid;
        if (!StepPeerActionThroughCoordinatorFrame(
                coordinator,
                peer,
                pair,
                GameplayActionRequested{
                    .kind = GameplayActionKind::TakeOffBackEntity,
                    .source_vid = peer_source_vid,
                    .target_vid = peer_existing_back_vid,
                },
                graphics,
                audio,
                label
            ) ||
            !StepPeerActionThroughCoordinatorFrame(
                coordinator,
                peer,
                pair,
                GameplayActionRequested{
                    .kind = GameplayActionKind::DropEntity,
                    .source_vid = peer_source_vid,
                    .target_vid = peer_existing_back_vid,
                },
                graphics,
                audio,
                label
            )) {
            return false;
        }
    }

    const std::optional<VID> peer_item_vid = SpawnCoordinatorEntityAndResolvePeer(
        coordinator,
        peer,
        pair,
        graphics,
        audio,
        item_type,
        peer_source_pos + Vec2::New(8.0F, 0.0F),
        label
    );
    if (!peer_item_vid.has_value() ||
        !PickupSpawnedEntityForFrameSmoke(
            coordinator,
            peer,
            pair,
            peer_source_vid,
            *peer_item_vid,
            graphics,
            audio,
            label
        ) ||
        !StepPeerActionThroughCoordinatorFrame(
            coordinator,
            peer,
            pair,
            GameplayActionRequested{
                .kind = GameplayActionKind::PutHeldEntityOnBack,
                .source_vid = peer_source_vid,
                .target_vid = *peer_item_vid,
            },
            graphics,
            audio,
            label
        )) {
        return false;
    }

    if (expect_peer_sprite_particles) {
        coordinator.particles.Clear();
        peer.particles.Clear();
    }
    const std::size_t peer_sprite_particle_count = peer.particles.sprite_particles.size();
    if (!StepPeerActionThroughCoordinatorFrame(
            coordinator,
            peer,
            pair,
            GameplayActionRequested{
                .kind = GameplayActionKind::UseBackEntity,
                .source_vid = peer_source_vid,
                .target_vid = *peer_item_vid,
                .param_a = 1,
            },
            graphics,
            audio,
            label
        ) ||
        !StepIdleFrameAndCompare(coordinator, peer, pair, graphics, audio, label)) {
        return false;
    }

    if (expect_peer_sprite_particles && peer.particles.sprite_particles.size() <= peer_sprite_particle_count) {
        std::cerr << "network frame smoke failed at " << label
                  << ": peer did not receive presentation particles\n";
        return false;
    }

    return StepPeerActionThroughCoordinatorFrame(
               coordinator,
               peer,
               pair,
               GameplayActionRequested{
                   .kind = GameplayActionKind::UseBackEntity,
                   .source_vid = peer_source_vid,
                   .target_vid = *peer_item_vid,
                   .param_a = 0,
               },
               graphics,
               audio,
               label
           ) &&
           StepPeerActionThroughCoordinatorFrame(
               coordinator,
               peer,
               pair,
               GameplayActionRequested{
                   .kind = GameplayActionKind::TakeOffBackEntity,
                   .source_vid = peer_source_vid,
                   .target_vid = *peer_item_vid,
               },
               graphics,
               audio,
               label
           ) &&
           StepPeerActionThroughCoordinatorFrame(
               coordinator,
               peer,
               pair,
               GameplayActionRequested{
                   .kind = GameplayActionKind::DropEntity,
                   .source_vid = peer_source_vid,
                   .target_vid = *peer_item_vid,
               },
               graphics,
               audio,
               label
           );
}

bool RunShopBuyScenario(
    State& coordinator,
    State& peer,
    PacketSmokePair& pair,
    VID peer_source_vid,
    Vec2 peer_source_pos,
    Graphics& graphics,
    Audio& audio
) {
    if (Entity* const buyer = peer.entity_manager.GetEntityMut(peer_source_vid)) {
        buyer->money = 1000;
    }
    if (const std::optional<VID> coordinator_source_vid =
            ResolveCoordinatorVidForPeerVid(coordinator, peer, peer_source_vid)) {
        if (Entity* const buyer = coordinator.entity_manager.GetEntityMut(*coordinator_source_vid)) {
            buyer->money = 1000;
        }
    }

    const std::optional<VID> peer_item_vid = SpawnCoordinatorEntityAndResolvePeer(
        coordinator,
        peer,
        pair,
        graphics,
        audio,
        EntityType::RopePile,
        peer_source_pos,
        "frame setup shop buy"
    );
    if (!peer_item_vid.has_value()) {
        return false;
    }
    if (const std::optional<VID> coordinator_item_vid =
            ResolveCoordinatorVidForPeerVid(coordinator, peer, *peer_item_vid)) {
        if (Entity* const item = coordinator.entity_manager.GetEntityMut(*coordinator_item_vid)) {
            ConfigureEntityAsBuyable(*item, 100);
            world_ops::PatchEntityState(coordinator, *item, *item);
        }
    }
    if (!DeliverCoordinatorPacketsToPeer(
            coordinator,
            peer,
            CoordinatorSmokeTransport(coordinator),
            PeerSmokeTransport(peer),
            pair.peer_endpoint,
            graphics,
            audio,
            "frame setup shop buyable"
        )) {
        return false;
    }

    return StepPeerActionThroughCoordinatorFrame(
        coordinator,
        peer,
        pair,
        GameplayActionRequested{
            .kind = GameplayActionKind::InteractEntity,
            .source_vid = peer_source_vid,
            .target_vid = *peer_item_vid,
        },
        graphics,
        audio,
        "frame shop buy"
    );
}

bool RunBombExplosionScenario(
    State& coordinator,
    State& peer,
    PacketSmokePair& pair,
    VID peer_source_vid,
    Graphics& graphics,
    Audio& audio
) {
    FillToolSlot(
        peer.entity_tools.EnsureToolSlot(peer_source_vid, 0),
        ToolKind::ThrowBomb,
        1,
        true
    );
    if (const std::optional<VID> coordinator_source_vid =
            ResolveCoordinatorVidForPeerVid(coordinator, peer, peer_source_vid)) {
        FillToolSlot(
            coordinator.entity_tools.EnsureToolSlot(*coordinator_source_vid, 0),
            ToolKind::ThrowBomb,
            1,
            true
        );
    }

    const IVec2 bomb_break_tile_pos = IVec2::New(6, 5);
    (void)world_ops::SetForegroundTile(coordinator, bomb_break_tile_pos, Tile::CaveDirt);
    if (!DeliverCoordinatorPacketsToPeer(
            coordinator,
            peer,
            CoordinatorSmokeTransport(coordinator),
            PeerSmokeTransport(peer),
            pair.peer_endpoint,
            graphics,
            audio,
            "frame setup bomb explosion"
        )) {
        return false;
    }

    if (!StepPeerActionThroughCoordinatorFrame(
            coordinator,
            peer,
            pair,
            GameplayActionRequested{
                .kind = GameplayActionKind::UseTool,
                .source_vid = peer_source_vid,
                .velocity = Vec2::New(0.0F, -2.0F),
                .param_a = 0,
            },
            graphics,
            audio,
            "frame bomb use"
        )) {
        return false;
    }

    constexpr int kBombExplosionFrames = 152;
    for (int i = 0; i < kBombExplosionFrames; ++i) {
        if (!StepIdleFrameAndCompare(
                coordinator,
                peer,
                pair,
                graphics,
                audio,
                "frame bomb explosion idle"
            )) {
            return false;
        }
    }
    return true;
}

bool StepStageTransitionFramesAndCompare(
    State& coordinator,
    State& peer,
    PacketSmokePair& pair,
    Graphics& graphics,
    Audio& audio
) {
    constexpr int kTransitionFrames = 64;
    for (int i = 0; i < kTransitionFrames; ++i) {
        StepSingleTick(peer, audio, graphics);
        if (!peer.net_session.pending_outbound_events.empty() &&
            !DeliverPeerPacketsToCoordinator(
                peer,
                coordinator,
                PeerSmokeTransport(peer),
                CoordinatorSmokeTransport(coordinator),
                pair.peer_endpoint,
                "frame stage transition peer->coordinator"
            )) {
            return false;
        }

        StepSingleTick(coordinator, audio, graphics);
        if (!DeliverCoordinatorPacketsToPeer(
                coordinator,
                peer,
                CoordinatorSmokeTransport(coordinator),
                PeerSmokeTransport(peer),
                pair.peer_endpoint,
                graphics,
                audio,
                "frame stage transition coordinator->peer",
                false
            )) {
            return false;
        }
    }

    if (coordinator.mode != Mode::Playing || peer.mode != Mode::Playing) {
        std::cerr << "network frame smoke failed at frame stage transition:"
                  << " coordinator/peer did not return to playing\n";
        return false;
    }
    if (coordinator.net_session.stage_instance_id != peer.net_session.stage_instance_id ||
        coordinator.net_session.stage_seed != peer.net_session.stage_seed ||
        coordinator.stage.quest_id != peer.stage.quest_id ||
        coordinator.stage.quest_stage_id != peer.stage.quest_stage_id) {
        std::cerr << "network frame smoke failed at frame stage transition:"
                  << " stage metadata mismatch\n";
        return false;
    }
    if (coordinator.stage.GetStageDims() != peer.stage.GetStageDims()) {
        std::cerr << "network frame smoke failed at frame stage transition:"
                  << " stage dims mismatch\n";
        return false;
    }
    for (unsigned int y = 0; y < coordinator.stage.GetTileHeight(); ++y) {
        for (unsigned int x = 0; x < coordinator.stage.GetTileWidth(); ++x) {
            if (coordinator.stage.GetTile(x, y) != peer.stage.GetTile(x, y) ||
                coordinator.stage.GetBackwallTile(x, y) != peer.stage.GetBackwallTile(x, y)) {
                std::cerr << "network frame smoke failed at frame stage transition:"
                          << " tile grid mismatch at " << x << "," << y << '\n';
                return false;
            }
        }
    }

    std::cout << "network frame smoke frame stage transition ok: stage="
              << coordinator.stage.quest_stage_id
              << " instance=" << coordinator.net_session.stage_instance_id
              << " seed=" << coordinator.net_session.stage_seed << '\n';
    return true;
}

bool RunHeldRespawnScenario(
    State& coordinator,
    State& peer,
    PacketSmokePair& pair,
    VID peer_source_vid,
    Vec2 peer_source_pos,
    Graphics& graphics,
    Audio& audio
) {
    const std::optional<VID> peer_holder_vid = SpawnCoordinatorEntityAndResolvePeer(
        coordinator,
        peer,
        pair,
        graphics,
        audio,
        EntityType::Player,
        peer_source_pos + Vec2::New(12.0F, 0.0F),
        "frame setup held respawn holder"
    );
    if (!peer_holder_vid.has_value()) {
        return false;
    }

    if (!StepPeerActionThroughCoordinatorFrame(
            coordinator,
            peer,
            pair,
            GameplayActionRequested{
                .kind = GameplayActionKind::PickupEntity,
                .source_vid = *peer_holder_vid,
                .target_vid = peer_source_vid,
            },
            graphics,
            audio,
            "frame pickup player before respawn",
            false
        )) {
        return false;
    }

    std::string respawn_status;
    if (!network::RespawnLocalPlayersAtEntrance(coordinator, graphics, &respawn_status)) {
        std::cerr << "network frame smoke failed: " << respawn_status << '\n';
        return false;
    }
    return DeliverCoordinatorPacketsToPeer(
        coordinator,
        peer,
        CoordinatorSmokeTransport(coordinator),
        PeerSmokeTransport(peer),
        pair.peer_endpoint,
        graphics,
        audio,
        "frame respawn while held"
    );
}

} // namespace

bool CheckNetworkFrameSmoke() {
    try {
        Graphics graphics;
        InitNetworkSmokeRuntimeTables(graphics);
        Audio audio;

        if (!RunBasicRemoteMovementSmoke(graphics, audio)) {
            return false;
        }

        constexpr std::uint32_t seed = 12345;
        State coordinator = State::New();
        State peer = State::New();
        ConfigureProtocolSmokeCoordinator(coordinator);
        ConfigureProtocolSmokePeer(peer);

        if (!LoadQuestStage(coordinator, "classic", "classic_mines_1", false, seed) ||
            !LoadQuestStage(peer, "classic", "classic_mines_1", false, seed)) {
            std::cerr << "network frame smoke failed: could not load test stages\n";
            return false;
        }
        ConfigureProtocolSmokeCoordinator(coordinator);
        ConfigureProtocolSmokePeer(peer);
        PacketSmokePair pair = MakePacketSmokePair();
        AttachPacketSmokeTransports(coordinator, peer, pair);

        if (!LinkAndCompareInitialStates(coordinator, peer)) {
            return false;
        }

        const Entity* const peer_source = FindFirstPlayerLikeEntity(peer);
        if (peer_source == nullptr) {
            std::cerr << "network frame smoke failed: missing peer source entity\n";
            return false;
        }
        const VID peer_source_vid = peer_source->vid;
        const Vec2 peer_source_pos = peer_source->pos;

        std::optional<std::size_t> peer_tool_slot = FindFirstUsableToolSlot(peer, peer_source_vid);
        if (const ToolSlot* const rope_slot = peer.entity_tools.FindToolSlot(peer_source_vid, 1);
            rope_slot != nullptr && rope_slot->active && rope_slot->count > 0 &&
            rope_slot->cooldown == 0) {
            peer_tool_slot = 1;
        }
        if (!peer_tool_slot.has_value()) {
            std::cerr << "network frame smoke failed: peer has no usable tool slot\n";
            return false;
        }

        if (!StepPeerActionThroughCoordinatorFrame(
                coordinator,
                peer,
                pair,
                GameplayActionRequested{
                    .kind = GameplayActionKind::UseTool,
                    .source_vid = peer_source_vid,
                    .velocity = Vec2::New(4.0F, -4.0F),
                    .param_a = static_cast<std::uint32_t>(*peer_tool_slot),
                },
                graphics,
                audio,
                "frame use tool"
            )) {
            return false;
        }

        const IVec2 break_tile_pos = IVec2::New(3, 3);
        (void)world_ops::SetForegroundTile(coordinator, break_tile_pos, Tile::CaveDirt);
        if (!DeliverCoordinatorPacketsToPeer(
                coordinator,
                peer,
                CoordinatorSmokeTransport(coordinator),
                PeerSmokeTransport(peer),
                pair.peer_endpoint,
                graphics,
                audio,
                "frame setup break tile"
            )) {
            return false;
        }
        if (!StepPeerActionThroughCoordinatorFrame(
                coordinator,
                peer,
                pair,
                GameplayActionRequested{
                    .kind = GameplayActionKind::BreakTile,
                    .source_vid = peer_source_vid,
                    .tile_pos = break_tile_pos,
                },
                graphics,
                audio,
                "frame break tile"
            )) {
            return false;
        }

        const std::optional<VID> peer_gold_vid = SpawnCoordinatorEntityAndResolvePeer(
            coordinator,
            peer,
            pair,
            graphics,
            audio,
            EntityType::Gold,
            peer_source_pos,
            "frame setup collect"
        );
        if (!peer_gold_vid.has_value() ||
            !StepPeerActionThroughCoordinatorFrame(
                coordinator,
                peer,
                pair,
                GameplayActionRequested{
                    .kind = GameplayActionKind::CollectEntity,
                    .source_vid = peer_source_vid,
                    .target_vid = *peer_gold_vid,
                },
                graphics,
                audio,
                "frame collect entity"
            )) {
            return false;
        }

        const std::optional<VID> peer_rock_vid = SpawnCoordinatorEntityAndResolvePeer(
            coordinator,
            peer,
            pair,
            graphics,
            audio,
            EntityType::Rock,
            peer_source_pos + Vec2::New(8.0F, 0.0F),
            "frame setup carry"
        );
        if (!peer_rock_vid.has_value() ||
            !StepPeerActionThroughCoordinatorFrame(
                coordinator,
                peer,
                pair,
                GameplayActionRequested{
                    .kind = GameplayActionKind::PickupEntity,
                    .source_vid = peer_source_vid,
                    .target_vid = *peer_rock_vid,
                },
                graphics,
                audio,
                "frame pickup entity"
            ) ||
            !StepPeerActionThroughCoordinatorFrame(
                coordinator,
                peer,
                pair,
                GameplayActionRequested{
                    .kind = GameplayActionKind::ThrowEntity,
                    .source_vid = peer_source_vid,
                    .target_vid = *peer_rock_vid,
                    .velocity = Vec2::New(2.0F, -3.0F),
                },
                graphics,
                audio,
                "frame throw entity"
            )) {
            return false;
        }

        const std::optional<VID> peer_drop_vid = SpawnCoordinatorEntityAndResolvePeer(
            coordinator,
            peer,
            pair,
            graphics,
            audio,
            EntityType::Rock,
            peer_source_pos + Vec2::New(8.0F, 0.0F),
            "frame setup drop"
        );
        if (!peer_drop_vid.has_value() ||
            !StepPeerActionThroughCoordinatorFrame(
                coordinator,
                peer,
                pair,
                GameplayActionRequested{
                    .kind = GameplayActionKind::PickupEntity,
                    .source_vid = peer_source_vid,
                    .target_vid = *peer_drop_vid,
                },
                graphics,
                audio,
                "frame pickup for drop"
            ) ||
            !StepPeerActionThroughCoordinatorFrame(
                coordinator,
                peer,
                pair,
                GameplayActionRequested{
                    .kind = GameplayActionKind::DropEntity,
                    .source_vid = peer_source_vid,
                    .target_vid = *peer_drop_vid,
                },
                graphics,
                audio,
                "frame drop entity"
            )) {
            return false;
        }

        if (!RunHeldUseScenario(
                coordinator,
                peer,
                pair,
                peer_source_vid,
                peer_source_pos,
                EntityType::BaseballBat,
                "frame held baseball bat use",
                true,
                graphics,
                audio
            ) ||
            !RunHeldUseScenario(
                coordinator,
                peer,
                pair,
                peer_source_vid,
                peer_source_pos,
                EntityType::Pistol,
                "frame held pistol use",
                true,
                graphics,
                audio
            ) ||
            !RunHeldUseScenario(
                coordinator,
                peer,
                pair,
                peer_source_vid,
                peer_source_pos,
                EntityType::WebCannon,
                "frame held web cannon use",
                true,
                graphics,
                audio
            ) ||
            !RunHeldUseScenario(
                coordinator,
                peer,
                pair,
                peer_source_vid,
                peer_source_pos,
                EntityType::Bow,
                "frame held bow use",
                true,
                graphics,
                audio
            ) ||
            !RunHeldUseScenario(
                coordinator,
                peer,
                pair,
                peer_source_vid,
                peer_source_pos,
                EntityType::Machete,
                "frame held machete use",
                true,
                graphics,
                audio
            ) ||
            !RunHeldUseScenario(
                coordinator,
                peer,
                pair,
                peer_source_vid,
                peer_source_pos,
                EntityType::Mattock,
                "frame held mattock use",
                true,
                graphics,
                audio
            ) ||
            !RunBackUseScenario(
                coordinator,
                peer,
                pair,
                peer_source_vid,
                peer_source_pos,
                EntityType::JetPack,
                "frame back jetpack use",
                true,
                graphics,
                audio
            ) ||
            !RunBackUseScenario(
                coordinator,
                peer,
                pair,
                peer_source_vid,
                peer_source_pos,
                EntityType::Cape,
                "frame back cape use",
                false,
                graphics,
                audio
            ) ||
            !RunShopBuyScenario(
                coordinator,
                peer,
                pair,
                peer_source_vid,
                peer_source_pos,
                graphics,
                audio
            )) {
            return false;
        }

        if (!RunBombExplosionScenario(
                coordinator,
                peer,
                pair,
                peer_source_vid,
                graphics,
                audio
            )) {
            return false;
        }

        const std::optional<VID> peer_block_vid = SpawnCoordinatorEntityAndResolvePeer(
            coordinator,
            peer,
            pair,
            graphics,
            audio,
            EntityType::Block,
            peer_source_pos + Vec2::New(5.0F, 0.0F),
            "frame setup push"
        );
        if (!peer_block_vid.has_value()) {
            return false;
        }
        if (Entity* const source = peer.entity_manager.GetEntityMut(peer_source_vid)) {
            source->grounded = true;
        }
        if (Entity* const source = coordinator.entity_manager.GetEntityMut(peer_source_vid)) {
            source->grounded = true;
        }
        if (!StepPeerActionThroughCoordinatorFrame(
                coordinator,
                peer,
                pair,
                GameplayActionRequested{
                    .kind = GameplayActionKind::PushEntity,
                    .source_vid = peer_source_vid,
                    .target_vid = *peer_block_vid,
                    .velocity = Vec2::New(0.5F, 0.0F),
                },
                graphics,
                audio,
                "frame push entity"
            )) {
            return false;
        }

        for (int i = 0; i < 3; ++i) {
            if (!StepIdleFrameAndCompare(
                    coordinator,
                    peer,
                    pair,
                    graphics,
                    audio,
                    "frame idle after tool"
                )) {
                return false;
            }
        }

        const std::optional<VID> peer_chest_vid = SpawnCoordinatorEntityAndResolvePeer(
            coordinator,
            peer,
            pair,
            graphics,
            audio,
            EntityType::Chest,
            peer_source_pos,
            "frame setup chest"
        );
        if (!peer_chest_vid.has_value() ||
            !StepPeerActionThroughCoordinatorFrame(
                coordinator,
                peer,
                pair,
                GameplayActionRequested{
                    .kind = GameplayActionKind::InteractEntity,
                    .source_vid = peer_source_vid,
                    .target_vid = *peer_chest_vid,
                },
                graphics,
                audio,
                "frame interact chest"
            )) {
            return false;
        }

        if (!RunHeldRespawnScenario(
                coordinator,
                peer,
                pair,
                peer_source_vid,
                peer_source_pos,
                graphics,
                audio
            )) {
            return false;
        }

        const Entity* const peer_exit = FindFirstEntityOfType(peer, EntityType::BasicExit);
        if (peer_exit == nullptr) {
            std::cerr << "network frame smoke failed: missing peer exit entity\n";
            return false;
        }
        if (!MovePeerAndCoordinatorEntityTo(
                coordinator,
                peer,
                peer_source_vid,
                peer_exit->pos
            )) {
            std::cerr << "network frame smoke failed: could not move peer source to exit\n";
            return false;
        }
        if (!StepPeerActionThroughCoordinatorFrame(
                coordinator,
                peer,
                pair,
                GameplayActionRequested{
                    .kind = GameplayActionKind::InteractEntity,
                    .source_vid = peer_source_vid,
                    .target_vid = peer_exit->vid,
                },
                graphics,
                audio,
                "frame exit interact",
                false
            )) {
            return false;
        }
        if (!StepStageTransitionFramesAndCompare(coordinator, peer, pair, graphics, audio)) {
            return false;
        }

        std::cout << "network frame smoke ok\n";
        return true;
    } catch (const std::exception& e) {
        std::cerr << "network frame smoke failed: " << e.what() << '\n';
        return false;
    }
}

} // namespace splonks

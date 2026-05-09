#include "cli_network_smoke.hpp"

#include "cli_network_smoke_internal.hpp"
#include "buying.hpp"
#include "frame_data_id.hpp"
#include "network/net_lobby_internal.hpp"
#include "network/net_lobby.hpp"
#include "network/net_progression.hpp"
#include "quest_stage_loader.hpp"
#include "step.hpp"
#include "tools/tool_archetype.hpp"
#include "world_ops.hpp"

#include <algorithm>
#include <cmath>
#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <vector>

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
    const char* label,
    bool compare_after_delivery = true
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
        label,
        compare_after_delivery
    );
}

bool LinkAndCompareInitialStates(State& coordinator, State& peer) {
    LinkMatchingEntitiesForActionSmoke(coordinator, peer);
    return CompareProtocolSmokeStates(coordinator, peer, "frame after load");
}

void LinkMatchingNonPlayerEntitiesForFrameSmoke(State& coordinator, State& peer) {
    coordinator.net_session.ClearStageEntityLinks();
    peer.net_session.ClearStageEntityLinks();

    network::NetEntityId next_stage_entity_id = 1000;
    const std::size_t count = std::min(
        coordinator.entity_manager.entities.size(),
        peer.entity_manager.entities.size()
    );
    for (std::size_t i = 0; i < count; ++i) {
        const Entity& coordinator_entity = coordinator.entity_manager.entities[i];
        const Entity& peer_entity = peer.entity_manager.entities[i];
        if (!coordinator_entity.active ||
            !peer_entity.active ||
            coordinator_entity.type_ != peer_entity.type_ ||
            IsPlayerLikeEntityType(coordinator_entity.type_)) {
            continue;
        }

        const network::NetEntityId entity_id = next_stage_entity_id++;
        coordinator.net_session.LinkEntity(entity_id, coordinator_entity.vid);
        peer.net_session.LinkEntity(entity_id, peer_entity.vid);
        coordinator.net_session.SetEntityOwner(entity_id, std::nullopt);
        peer.net_session.SetEntityOwner(entity_id, std::nullopt);
    }
}

bool ConfigureDualPlayerFrameSmoke(
    State& coordinator,
    State& peer,
    const Graphics& graphics,
    VID& coordinator_host_vid,
    VID& coordinator_peer_vid,
    VID& peer_host_vid,
    VID& peer_player_vid
) {
    constexpr PlayerId kHostPlayerId = 1;
    constexpr PlayerId kPeerPlayerId = 2;
    const Vec2 host_pos = Vec2::New(80.0F, 96.0F);
    const Vec2 peer_pos = Vec2::New(88.0F, 96.0F);

    network::EnsureSpawnedPlayer(coordinator, kHostPlayerId, true, true, host_pos, graphics);
    network::EnsureSpawnedPlayer(coordinator, kPeerPlayerId, false, false, peer_pos, graphics);
    network::EnsureSpawnedPlayer(peer, kHostPlayerId, false, false, host_pos, graphics);
    network::EnsureSpawnedPlayer(peer, kPeerPlayerId, true, true, peer_pos, graphics);

    const PlayerSlot* const coordinator_host_slot = coordinator.players.Find(kHostPlayerId);
    const PlayerSlot* const coordinator_peer_slot = coordinator.players.Find(kPeerPlayerId);
    const PlayerSlot* const peer_host_slot = peer.players.Find(kHostPlayerId);
    const PlayerSlot* const peer_player_slot = peer.players.Find(kPeerPlayerId);
    if (coordinator_host_slot == nullptr ||
        coordinator_peer_slot == nullptr ||
        peer_host_slot == nullptr ||
        peer_player_slot == nullptr ||
        !coordinator_host_slot->entity_vid.has_value() ||
        !coordinator_peer_slot->entity_vid.has_value() ||
        !peer_host_slot->entity_vid.has_value() ||
        !peer_player_slot->entity_vid.has_value()) {
        return false;
    }

    coordinator_host_vid = *coordinator_host_slot->entity_vid;
    coordinator_peer_vid = *coordinator_peer_slot->entity_vid;
    peer_host_vid = *peer_host_slot->entity_vid;
    peer_player_vid = *peer_player_slot->entity_vid;
    for (const VID vid : {coordinator_host_vid, coordinator_peer_vid}) {
        if (Entity* const entity = coordinator.entity_manager.GetEntityMut(vid)) {
            entity->vel = Vec2::New(0.0F, 0.0F);
            entity->acc = Vec2::New(0.0F, 0.0F);
            entity->grounded = true;
            entity->fall_timer = 0;
            entity->stun_timer = 0;
            entity->condition = EntityCondition::Normal;
        }
    }
    for (const VID vid : {peer_host_vid, peer_player_vid}) {
        if (Entity* const entity = peer.entity_manager.GetEntityMut(vid)) {
            entity->vel = Vec2::New(0.0F, 0.0F);
            entity->acc = Vec2::New(0.0F, 0.0F);
            entity->grounded = true;
            entity->fall_timer = 0;
            entity->stun_timer = 0;
            entity->condition = EntityCondition::Normal;
        }
    }

    LinkMatchingNonPlayerEntitiesForFrameSmoke(coordinator, peer);
    coordinator.net_session.LinkEntity(network::MakePlayerNetEntityId(kHostPlayerId), coordinator_host_vid);
    coordinator.net_session.LinkEntity(network::MakePlayerNetEntityId(kPeerPlayerId), coordinator_peer_vid);
    peer.net_session.LinkEntity(network::MakePlayerNetEntityId(kHostPlayerId), peer_host_vid);
    peer.net_session.LinkEntity(network::MakePlayerNetEntityId(kPeerPlayerId), peer_player_vid);
    return true;
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
    peer_player_mut->acc = Vec2::New(9.0F, -7.0F);
    peer_player_mut->size = Vec2::New(99.0F, 88.0F);
    peer_player_mut->rotation = 1.25F;
    peer_player_mut->health = 123;
    peer_player_mut->fall_timer = 99;
    peer_player_mut->stun_timer = 77;
    peer_player_mut->projectile_contact_timer = 55;
    peer_player_mut->movement_flags = 0;
    SetMovementFlag(*peer_player_mut, EntityMovementFlag::Climbing, true);
    SetMovementFlag(*peer_player_mut, EntityMovementFlag::Hanging, true);
    peer_player_mut->jump_hold_gravity_frames_remaining = 44;
    peer_player_mut->jump_delay_frame_count = 33;
    peer_player_mut->climb_detach_cooldown = 22;
    peer_player_mut->hang_count = 11;
    peer_player_mut->hang_side = LeftOrRight::Right;
    peer_player_mut->holding_timer = 66;
    peer_player_mut->bomb_throw_delay_countdown = 55;
    peer_player_mut->rope_throw_delay_countdown = 44;
    peer_player_mut->attack_delay_countdown = 33;
    peer_player_mut->equip_delay_countdown = 22;
    peer_player_mut->thrown_immunity_timer = 11;
    peer_player_mut->condition = EntityCondition::Stunned;
    peer_player_mut->grounded = false;
    peer_player_mut->frame_data_animator.SetAnimation(frame_data_ids::PlayerStunned);
    peer_player_mut->frame_data_animator.current_frame = 1;
    peer_player_mut->frame_data_animator.current_time = 7.0F;
    peer_player_mut->frame_data_animator.speed = 0.25F;
    peer_player_mut->frame_data_animator.animate = false;
    peer_player_mut->frame_data_animator.loop = false;
    peer_player_mut->frame_data_animator.finished = true;
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
        repaired_peer_player->acc != coordinator_player->acc ||
        repaired_peer_player->size != coordinator_player->size ||
        std::fabs(repaired_peer_player->rotation - coordinator_player->rotation) > 0.001F ||
        repaired_peer_player->fall_timer != coordinator_player->fall_timer ||
        repaired_peer_player->stun_timer != coordinator_player->stun_timer ||
        repaired_peer_player->projectile_contact_timer != coordinator_player->projectile_contact_timer ||
        repaired_peer_player->movement_flags != coordinator_player->movement_flags ||
        repaired_peer_player->jump_hold_gravity_frames_remaining !=
            coordinator_player->jump_hold_gravity_frames_remaining ||
        repaired_peer_player->jump_delay_frame_count != coordinator_player->jump_delay_frame_count ||
        repaired_peer_player->climb_detach_cooldown != coordinator_player->climb_detach_cooldown ||
        repaired_peer_player->hang_count != coordinator_player->hang_count ||
        repaired_peer_player->hang_side != coordinator_player->hang_side ||
        repaired_peer_player->holding_timer != coordinator_player->holding_timer ||
        repaired_peer_player->bomb_throw_delay_countdown !=
            coordinator_player->bomb_throw_delay_countdown ||
        repaired_peer_player->rope_throw_delay_countdown !=
            coordinator_player->rope_throw_delay_countdown ||
        repaired_peer_player->attack_delay_countdown != coordinator_player->attack_delay_countdown ||
        repaired_peer_player->equip_delay_countdown != coordinator_player->equip_delay_countdown ||
        repaired_peer_player->thrown_immunity_timer != coordinator_player->thrown_immunity_timer ||
        repaired_peer_player->frame_data_animator.animation_id !=
            coordinator_player->frame_data_animator.animation_id ||
        repaired_peer_player->frame_data_animator.current_frame !=
            coordinator_player->frame_data_animator.current_frame ||
        std::fabs(
            repaired_peer_player->frame_data_animator.current_time -
            coordinator_player->frame_data_animator.current_time
        ) > 0.001F ||
        std::fabs(
            repaired_peer_player->frame_data_animator.speed -
            coordinator_player->frame_data_animator.speed
        ) > 0.001F ||
        repaired_peer_player->frame_data_animator.animate !=
            coordinator_player->frame_data_animator.animate ||
        repaired_peer_player->frame_data_animator.loop !=
            coordinator_player->frame_data_animator.loop ||
        repaired_peer_player->frame_data_animator.finished !=
            coordinator_player->frame_data_animator.finished ||
        repaired_peer_player->condition != coordinator_player->condition ||
        repaired_peer_player->grounded != coordinator_player->grounded) {
        std::cerr << "network frame smoke failed at frame basic local player state repair:"
                  << " coordinator health=" << coordinator_player->health
                  << " acc=" << coordinator_player->acc.x << "," << coordinator_player->acc.y
                  << " size=" << coordinator_player->size.x << "," << coordinator_player->size.y
                  << " rotation=" << coordinator_player->rotation
                  << " fall=" << coordinator_player->fall_timer
                  << " stun=" << coordinator_player->stun_timer
                  << " movement=" << coordinator_player->movement_flags
                  << " jump_hold=" << coordinator_player->jump_hold_gravity_frames_remaining
                  << " jump_delay=" << coordinator_player->jump_delay_frame_count
                  << " climb_detach=" << coordinator_player->climb_detach_cooldown
                  << " hang_count=" << coordinator_player->hang_count
                  << " hang_side="
                  << (coordinator_player->hang_side.has_value()
                          ? static_cast<int>(*coordinator_player->hang_side)
                          : -1)
                  << " holding_timer=" << coordinator_player->holding_timer
                  << " bomb_delay=" << coordinator_player->bomb_throw_delay_countdown
                  << " rope_delay=" << coordinator_player->rope_throw_delay_countdown
                  << " attack_delay=" << coordinator_player->attack_delay_countdown
                  << " equip_delay=" << coordinator_player->equip_delay_countdown
                  << " thrown_immunity=" << coordinator_player->thrown_immunity_timer
                  << " anim=" << coordinator_player->frame_data_animator.animation_id
                  << " frame/time/speed=" << coordinator_player->frame_data_animator.current_frame
                  << "," << coordinator_player->frame_data_animator.current_time
                  << "," << coordinator_player->frame_data_animator.speed
                  << " condition=" << static_cast<int>(coordinator_player->condition)
                  << " grounded=" << coordinator_player->grounded
                  << " peer health=" << repaired_peer_player->health
                  << " acc=" << repaired_peer_player->acc.x << "," << repaired_peer_player->acc.y
                  << " size=" << repaired_peer_player->size.x << "," << repaired_peer_player->size.y
                  << " rotation=" << repaired_peer_player->rotation
                  << " fall=" << repaired_peer_player->fall_timer
                  << " stun=" << repaired_peer_player->stun_timer
                  << " movement=" << repaired_peer_player->movement_flags
                  << " jump_hold=" << repaired_peer_player->jump_hold_gravity_frames_remaining
                  << " jump_delay=" << repaired_peer_player->jump_delay_frame_count
                  << " climb_detach=" << repaired_peer_player->climb_detach_cooldown
                  << " hang_count=" << repaired_peer_player->hang_count
                  << " hang_side="
                  << (repaired_peer_player->hang_side.has_value()
                          ? static_cast<int>(*repaired_peer_player->hang_side)
                          : -1)
                  << " holding_timer=" << repaired_peer_player->holding_timer
                  << " bomb_delay=" << repaired_peer_player->bomb_throw_delay_countdown
                  << " rope_delay=" << repaired_peer_player->rope_throw_delay_countdown
                  << " attack_delay=" << repaired_peer_player->attack_delay_countdown
                  << " equip_delay=" << repaired_peer_player->equip_delay_countdown
                  << " thrown_immunity=" << repaired_peer_player->thrown_immunity_timer
                  << " anim=" << repaired_peer_player->frame_data_animator.animation_id
                  << " frame/time/speed=" << repaired_peer_player->frame_data_animator.current_frame
                  << "," << repaired_peer_player->frame_data_animator.current_time
                  << "," << repaired_peer_player->frame_data_animator.speed
                  << " condition=" << static_cast<int>(repaired_peer_player->condition)
                  << " grounded=" << repaired_peer_player->grounded << '\n';
        return false;
    }

    std::cout << "network frame smoke basic remote movement ok: distance="
              << distance << " repair_distance=" << repair_distance << '\n';
    return true;
}

struct DelayedSnapshotPacket {
    int deliver_frame = 0;
    network::UdpPacket packet;
};

bool QueuePeerSnapshotWithDelay(
    State& peer,
    PacketSmokePair& pair,
    std::vector<DelayedSnapshotPacket>& queue,
    int current_frame,
    int delay_frames,
    const char* label
) {
    network::SendSnapshotsToEndpoint(peer, PeerSmokeTransport(peer), pair.coordinator_endpoint);
    std::vector<network::UdpPacket> packets = TakeCapturedSnapshotPackets(PeerSmokeTransport(peer));
    if (packets.empty()) {
        std::cerr << "network frame smoke failed at " << label
                  << ": peer emitted no delayed snapshot packet\n";
        return false;
    }
    for (network::UdpPacket& packet : packets) {
        queue.push_back(DelayedSnapshotPacket{
            .deliver_frame = current_frame + delay_frames,
            .packet = std::move(packet),
        });
    }
    return true;
}

bool DeliverDuePeerSnapshots(
    State& coordinator,
    const Graphics& graphics,
    std::vector<DelayedSnapshotPacket>& queue,
    int current_frame,
    const char* label
) {
    for (std::size_t i = 0; i < queue.size();) {
        if (queue[i].deliver_frame > current_frame) {
            ++i;
            continue;
        }

        const network::UdpPacket& packet = queue[i].packet;
        const std::optional<network::PlayerSnapshotsPacket> snapshots =
            network::TryDecodePlayerSnapshots(packet.bytes.data(), packet.size);
        if (!snapshots.has_value()) {
            std::cerr << "network frame smoke failed at " << label
                      << ": delayed peer packet was not a player snapshot\n";
            return false;
        }
        network::ApplyPlayerSnapshots(
            coordinator,
            graphics,
            CoordinatorSmokeTransport(coordinator),
            *snapshots
        );
        queue.erase(queue.begin() + static_cast<std::ptrdiff_t>(i));
    }
    return true;
}

bool RunBasicMovementLatencySmoke(Graphics& graphics, Audio& audio) {
    constexpr std::uint32_t seed = 12345;
    constexpr int kLatencyFrames = 4;
    constexpr int kInputFrames = 36;
    State coordinator = State::New();
    State peer = State::New();
    ConfigureProtocolSmokeCoordinator(coordinator);
    ConfigureProtocolSmokePeer(peer);

    if (!LoadQuestStage(coordinator, "classic", "classic_mines_1", false, seed) ||
        !LoadQuestStage(peer, "classic", "classic_mines_1", false, seed)) {
        std::cerr << "network frame smoke failed: could not load latency movement stages\n";
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
        std::cerr << "network frame smoke failed: could not configure latency movement player\n";
        return false;
    }

    std::vector<DelayedSnapshotPacket> delayed_snapshots;
    int frame = 0;
    for (; frame < kInputFrames; ++frame) {
        const bool jump = frame >= 1 && frame <= 5;
        SetMovementSmokeInput(peer, true, true, jump);
        StepSingleTick(peer, audio, graphics);
        if (!QueuePeerSnapshotWithDelay(
                peer,
                pair,
                delayed_snapshots,
                frame,
                kLatencyFrames,
                "frame latency movement queue"
            ) ||
            !DeliverDuePeerSnapshots(
                coordinator,
                graphics,
                delayed_snapshots,
                frame,
                "frame latency movement deliver"
            )) {
            return false;
        }
        StepSingleTick(coordinator, audio, graphics);
    }

    for (int settle = 0; settle < kLatencyFrames + 6; ++settle, ++frame) {
        SetMovementSmokeInput(peer, false, false, false);
        StepSingleTick(peer, audio, graphics);
        if (!QueuePeerSnapshotWithDelay(
                peer,
                pair,
                delayed_snapshots,
                frame,
                kLatencyFrames,
                "frame latency movement settle queue"
            ) ||
            !DeliverDuePeerSnapshots(
                coordinator,
                graphics,
                delayed_snapshots,
                frame,
                "frame latency movement settle deliver"
            )) {
            return false;
        }
        StepSingleTick(coordinator, audio, graphics);
    }

    while (!delayed_snapshots.empty()) {
        if (!DeliverDuePeerSnapshots(
                coordinator,
                graphics,
                delayed_snapshots,
                frame++,
                "frame latency movement drain"
            )) {
            return false;
        }
        StepSingleTick(coordinator, audio, graphics);
    }

    if (!DeliverCoordinatorSnapshotsToPeer(
            coordinator,
            peer,
            pair,
            graphics,
            "frame latency movement repair"
        )) {
        return false;
    }
    network::StepRemotePlayerInterpolation(peer, PeerSmokeTransport(peer), graphics);

    const Entity* const coordinator_player =
        coordinator.entity_manager.GetEntity(coordinator_player_vid);
    const Entity* const peer_player = peer.entity_manager.GetEntity(peer_player_vid);
    if (coordinator_player == nullptr || peer_player == nullptr) {
        std::cerr << "network frame smoke failed: missing latency movement player after repair\n";
        return false;
    }
    const Vec2 delta = coordinator_player->pos - peer_player->pos;
    const float repair_distance = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    constexpr float kMaxRepairDistancePx = 2.0F;
    if (repair_distance > kMaxRepairDistancePx ||
        coordinator_player->condition != peer_player->condition ||
        coordinator_player->fall_timer != peer_player->fall_timer ||
        coordinator_player->stun_timer != peer_player->stun_timer ||
        coordinator_player->movement_flags != peer_player->movement_flags ||
        coordinator_player->grounded != peer_player->grounded) {
        std::cerr << "network frame smoke failed at frame latency movement repair:"
                  << " coordinator pos=" << coordinator_player->pos.x << ","
                  << coordinator_player->pos.y
                  << " peer pos=" << peer_player->pos.x << "," << peer_player->pos.y
                  << " distance=" << repair_distance
                  << " coordinator condition=" << static_cast<int>(coordinator_player->condition)
                  << " peer condition=" << static_cast<int>(peer_player->condition)
                  << " coordinator fall=" << coordinator_player->fall_timer
                  << " peer fall=" << peer_player->fall_timer
                  << " coordinator stun=" << coordinator_player->stun_timer
                  << " peer stun=" << peer_player->stun_timer
                  << '\n';
        return false;
    }

    std::cout << "network frame smoke basic movement under latency ok: repair_distance="
              << repair_distance << '\n';
    return true;
}

bool RunPlayerCorrectionPolicySmoke(Graphics& graphics) {
    constexpr std::uint32_t seed = 12345;
    State coordinator = State::New();
    State peer = State::New();
    ConfigureProtocolSmokeCoordinator(coordinator);
    ConfigureProtocolSmokePeer(peer);

    if (!LoadQuestStage(coordinator, "classic", "classic_mines_1", false, seed) ||
        !LoadQuestStage(peer, "classic", "classic_mines_1", false, seed)) {
        std::cerr << "network frame smoke failed: could not load correction policy stages\n";
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
        std::cerr << "network frame smoke failed: could not configure correction policy player\n";
        return false;
    }

    Entity* const coordinator_player = coordinator.entity_manager.GetEntityMut(coordinator_player_vid);
    Entity* peer_player = peer.entity_manager.GetEntityMut(peer_player_vid);
    if (coordinator_player == nullptr || peer_player == nullptr) {
        std::cerr << "network frame smoke failed: missing correction policy player\n";
        return false;
    }

    auto& peer_transport = PeerSmokeTransport(peer);
    peer_transport.remote_interpolation_delay_frames = 0;
    peer_transport.remote_interpolation_strength = 0.5F;
    peer_transport.remote_snap_distance = 16.0F;

    coordinator_player->pos = Vec2::New(64.0F, 114.0F);
    peer_player->pos = coordinator_player->pos + Vec2::New(8.0F, 0.0F);
    if (!DeliverCoordinatorSnapshotsToPeer(
            coordinator,
            peer,
            pair,
            graphics,
            "frame correction policy smooth snapshot"
        )) {
        return false;
    }
    network::StepRemotePlayerInterpolation(peer, peer_transport, graphics);
    const Entity* const smoothed_player = peer.entity_manager.GetEntity(peer_player_vid);
    if (smoothed_player == nullptr) {
        std::cerr << "network frame smoke failed: missing smoothed correction player\n";
        return false;
    }
    const float smooth_distance = std::fabs(smoothed_player->pos.x - coordinator_player->pos.x);
    if (!(smooth_distance > 0.01F && smooth_distance < 8.0F)) {
        std::cerr << "network frame smoke failed at frame correction policy smooth:"
                  << " expected partial repair, distance=" << smooth_distance << '\n';
        return false;
    }

    peer_player = peer.entity_manager.GetEntityMut(peer_player_vid);
    if (peer_player == nullptr) {
        std::cerr << "network frame smoke failed: missing snap correction player\n";
        return false;
    }
    peer_player->pos = coordinator_player->pos + Vec2::New(80.0F, 0.0F);
    if (!DeliverCoordinatorSnapshotsToPeer(
            coordinator,
            peer,
            pair,
            graphics,
            "frame correction policy snap snapshot"
        )) {
        return false;
    }
    network::StepRemotePlayerInterpolation(peer, peer_transport, graphics);
    const Entity* const snapped_player = peer.entity_manager.GetEntity(peer_player_vid);
    if (snapped_player == nullptr) {
        std::cerr << "network frame smoke failed: missing snapped correction player\n";
        return false;
    }
    const Vec2 snap_delta = snapped_player->pos - coordinator_player->pos;
    const float snap_distance = std::sqrt(snap_delta.x * snap_delta.x + snap_delta.y * snap_delta.y);
    if (snap_distance > 0.01F) {
        std::cerr << "network frame smoke failed at frame correction policy snap:"
                  << " expected hard snap, distance=" << snap_distance << '\n';
        return false;
    }

    std::cout << "network frame smoke player correction policy ok:"
              << " smooth_distance=" << smooth_distance
              << " snap_distance=" << snap_distance << '\n';
    return true;
}

bool RunPlayerMovementStateRepairSmoke(Graphics& graphics, Audio& audio) {
    constexpr std::uint32_t seed = 12345;
    State coordinator = State::New();
    State peer = State::New();
    ConfigureProtocolSmokeCoordinator(coordinator);
    ConfigureProtocolSmokePeer(peer);

    if (!LoadQuestStage(coordinator, "classic", "classic_mines_1", false, seed) ||
        !LoadQuestStage(peer, "classic", "classic_mines_1", false, seed)) {
        std::cerr << "network frame smoke failed: could not load movement state stages\n";
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
        std::cerr << "network frame smoke failed: could not configure movement state player\n";
        return false;
    }

    Entity* coordinator_player = coordinator.entity_manager.GetEntityMut(coordinator_player_vid);
    Entity* peer_player = peer.entity_manager.GetEntityMut(peer_player_vid);
    if (coordinator_player == nullptr || peer_player == nullptr) {
        std::cerr << "network frame smoke failed: missing movement state player\n";
        return false;
    }

    auto& peer_transport = PeerSmokeTransport(peer);
    peer_transport.remote_interpolation_delay_frames = 0;
    peer_transport.remote_interpolation_strength = 1.0F;
    peer_transport.remote_snap_distance = 32.0F;

    coordinator_player->movement_flags = 0;
    SetMovementFlag(*coordinator_player, EntityMovementFlag::Climbing, true);
    coordinator_player->grounded = false;
    coordinator_player->fall_timer = 0;
    coordinator_player->stun_timer = 0;
    coordinator_player->pos = Vec2::New(64.0F, 96.0F);
    peer_player->movement_flags = 0;
    peer_player->grounded = true;
    if (!DeliverCoordinatorSnapshotsToPeer(
            coordinator,
            peer,
            pair,
            graphics,
            "frame movement state climbing snapshot"
        )) {
        return false;
    }
    network::StepRemotePlayerInterpolation(peer, peer_transport, graphics);
    peer_player = peer.entity_manager.GetEntityMut(peer_player_vid);
    if (peer_player == nullptr || !HasMovementFlag(*peer_player, EntityMovementFlag::Climbing) ||
        peer_player->grounded || peer_player->fall_timer != 0 || peer_player->stun_timer != 0) {
        std::cerr << "network frame smoke failed at frame movement state climbing repair\n";
        return false;
    }

    coordinator_player = coordinator.entity_manager.GetEntityMut(coordinator_player_vid);
    if (coordinator_player == nullptr) {
        std::cerr << "network frame smoke failed: missing coordinator hanging player\n";
        return false;
    }
    coordinator_player->movement_flags = 0;
    SetMovementFlag(*coordinator_player, EntityMovementFlag::Hanging, true);
    coordinator_player->hang_count = 6;
    coordinator_player->hang_side = LeftOrRight::Left;
    coordinator_player->pos = Vec2::New(80.0F, 96.0F);
    peer_player->movement_flags = 0;
    peer_player->hang_count = 0;
    peer_player->hang_side.reset();
    if (!DeliverCoordinatorSnapshotsToPeer(
            coordinator,
            peer,
            pair,
            graphics,
            "frame movement state hanging snapshot"
        )) {
        return false;
    }
    network::StepRemotePlayerInterpolation(peer, peer_transport, graphics);
    peer_player = peer.entity_manager.GetEntityMut(peer_player_vid);
    if (peer_player == nullptr || !HasMovementFlag(*peer_player, EntityMovementFlag::Hanging) ||
        peer_player->hang_count != coordinator_player->hang_count ||
        peer_player->hang_side != coordinator_player->hang_side) {
        std::cerr << "network frame smoke failed at frame movement state hanging repair\n";
        return false;
    }

    coordinator_player = coordinator.entity_manager.GetEntityMut(coordinator_player_vid);
    peer_player = peer.entity_manager.GetEntityMut(peer_player_vid);
    if (coordinator_player == nullptr || peer_player == nullptr) {
        std::cerr << "network frame smoke failed: missing water state player\n";
        return false;
    }
    (void)AddEffect(*coordinator_player, EffectId::InWater);
    RemoveEffect(*peer_player, EffectId::InWater);
    world_ops::PatchPlayerState(coordinator, *coordinator_player);
    if (!DeliverCoordinatorPacketsToPeer(
            coordinator,
            peer,
            CoordinatorSmokeTransport(coordinator),
            PeerSmokeTransport(peer),
            pair.peer_endpoint,
            graphics,
            audio,
            "frame movement state water effect",
            false
        )) {
        return false;
    }
    peer_player = peer.entity_manager.GetEntityMut(peer_player_vid);
    if (peer_player == nullptr || !HasEffect(*peer_player, EffectId::InWater)) {
        std::cerr << "network frame smoke failed at frame movement state water effect repair\n";
        return false;
    }

    std::cout << "network frame smoke player movement state repair ok\n";
    return true;
}

bool RunPlayerBodyLossRecoverySmoke(Graphics& graphics) {
    constexpr std::uint32_t seed = 12345;
    State coordinator = State::New();
    State peer = State::New();
    ConfigureProtocolSmokeCoordinator(coordinator);
    ConfigureProtocolSmokePeer(peer);

    if (!LoadQuestStage(coordinator, "classic", "classic_mines_1", false, seed) ||
        !LoadQuestStage(peer, "classic", "classic_mines_1", false, seed)) {
        std::cerr << "network frame smoke failed: could not load body loss recovery stages\n";
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
        std::cerr << "network frame smoke failed: could not configure body loss recovery player\n";
        return false;
    }

    Entity* const coordinator_player = coordinator.entity_manager.GetEntityMut(coordinator_player_vid);
    Entity* peer_player = peer.entity_manager.GetEntityMut(peer_player_vid);
    if (coordinator_player == nullptr || peer_player == nullptr) {
        std::cerr << "network frame smoke failed: missing body loss recovery player\n";
        return false;
    }

    coordinator_player->pos = Vec2::New(64.0F, 114.0F);
    coordinator_player->vel = Vec2::New(0.0F, 0.0F);
    coordinator_player->acc = Vec2::New(0.0F, 0.0F);
    coordinator_player->health = 400;
    coordinator_player->condition = EntityCondition::Normal;
    coordinator_player->fall_timer = 0;
    coordinator_player->stun_timer = 0;
    coordinator_player->coyote_time = 3;
    coordinator_player->grounded = true;
    coordinator_player->has_physics = true;
    coordinator_player->can_collide = true;
    coordinator_player->movement_flags = 0;

    peer_player->pos = Vec2::New(160.0F, 24.0F);
    peer_player->vel = Vec2::New(12.0F, -18.0F);
    peer_player->acc = Vec2::New(7.0F, 9.0F);
    peer_player->health = 0;
    peer_player->condition = EntityCondition::Dead;
    peer_player->fall_timer = 240;
    peer_player->stun_timer = 240;
    peer_player->coyote_time = 0;
    peer_player->grounded = false;
    peer_player->has_physics = false;
    peer_player->can_collide = false;
    peer_player->movement_flags = 0;
    SetMovementFlag(*peer_player, EntityMovementFlag::Climbing, true);
    SetMovementFlag(*peer_player, EntityMovementFlag::Hanging, true);

    auto& peer_transport = PeerSmokeTransport(peer);
    peer_transport.remote_interpolation_delay_frames = 0;
    peer_transport.remote_interpolation_strength = 1.0F;
    peer_transport.remote_snap_distance = 32.0F;

    network::SendSnapshotsToEndpoint(
        coordinator,
        CoordinatorSmokeTransport(coordinator),
        pair.peer_endpoint
    );
    const std::vector<network::UdpPacket> dropped_packets =
        TakeCapturedSnapshotPackets(CoordinatorSmokeTransport(coordinator));
    if (dropped_packets.empty()) {
        std::cerr << "network frame smoke failed at frame body loss recovery:"
                  << " coordinator emitted no droppable snapshot packet\n";
        return false;
    }

    if (!DeliverCoordinatorSnapshotsToPeer(
            coordinator,
            peer,
            pair,
            graphics,
            "frame body loss recovery repair snapshot"
        )) {
        return false;
    }
    network::StepRemotePlayerInterpolation(peer, peer_transport, graphics);
    peer_player = peer.entity_manager.GetEntityMut(peer_player_vid);
    if (peer_player == nullptr) {
        std::cerr << "network frame smoke failed: missing repaired body loss recovery player\n";
        return false;
    }

    const Vec2 repair_delta = peer_player->pos - coordinator_player->pos;
    const float repair_distance =
        std::sqrt(repair_delta.x * repair_delta.x + repair_delta.y * repair_delta.y);
    if (repair_distance > 0.01F ||
        peer_player->vel != coordinator_player->vel ||
        peer_player->acc != coordinator_player->acc ||
        peer_player->health != coordinator_player->health ||
        peer_player->condition != EntityCondition::Normal ||
        peer_player->fall_timer != 0 ||
        peer_player->stun_timer != 0 ||
        peer_player->coyote_time != coordinator_player->coyote_time ||
        peer_player->grounded != coordinator_player->grounded ||
        peer_player->has_physics != coordinator_player->has_physics ||
        peer_player->can_collide != coordinator_player->can_collide ||
        peer_player->movement_flags != coordinator_player->movement_flags) {
        std::cerr << "network frame smoke failed at frame body loss recovery repair:"
                  << " distance=" << repair_distance
                  << " health=" << peer_player->health
                  << " condition=" << static_cast<int>(peer_player->condition)
                  << " fall=" << peer_player->fall_timer
                  << " stun=" << peer_player->stun_timer
                  << " grounded=" << peer_player->grounded
                  << " has_physics=" << peer_player->has_physics
                  << " can_collide=" << peer_player->can_collide
                  << " movement=" << peer_player->movement_flags << '\n';
        return false;
    }

    std::cout << "network frame smoke player body loss recovery ok\n";
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
    const char* label,
    bool compare_after_delivery = true
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
            label,
            compare_after_delivery
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

std::size_t CountActiveEntitiesOfType(const State& state, EntityType type_) {
    std::size_t count = 0;
    for (const Entity& entity : state.entity_manager.entities) {
        if (entity.active && entity.type_ == type_) {
            ++count;
        }
    }
    return count;
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
            label,
            false
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

bool BuildDualPlayerFrameFixture(
    State& coordinator,
    State& peer,
    PacketSmokePair& pair,
    VID& coordinator_host_vid,
    VID& coordinator_peer_vid,
    VID& peer_host_vid,
    VID& peer_player_vid,
    Graphics& graphics
);

bool RunChanceShopRollScenario(
    Graphics& graphics,
    Audio& audio,
    int forced_roll,
    unsigned int expected_money,
    bool expect_prize,
    const char* label
) {
    State coordinator = State::New();
    State peer = State::New();
    PacketSmokePair pair;
    VID coordinator_host_vid;
    VID coordinator_peer_vid;
    VID peer_host_vid;
    VID peer_player_vid;
    if (!BuildDualPlayerFrameFixture(
            coordinator,
            peer,
            pair,
            coordinator_host_vid,
            coordinator_peer_vid,
            peer_host_vid,
            peer_player_vid,
            graphics
        )) {
        return false;
    }

    const Vec2 table_pos = Vec2::New(96.0F, 96.0F);
    const std::optional<VID> peer_table_vid = SpawnCoordinatorEntityAndResolvePeer(
        coordinator,
        peer,
        pair,
        graphics,
        audio,
        EntityType::CrapsTable,
        table_pos,
        label,
        false
    );
    const std::optional<VID> peer_dice_vid = SpawnCoordinatorEntityAndResolvePeer(
        coordinator,
        peer,
        pair,
        graphics,
        audio,
        EntityType::Dice,
        table_pos,
        label,
        false
    );
    const std::optional<VID> peer_prize_vid = SpawnCoordinatorEntityAndResolvePeer(
        coordinator,
        peer,
        pair,
        graphics,
        audio,
        EntityType::JetPack,
        table_pos + Vec2::New(0.0F, -24.0F),
        label,
        false
    );
    if (!peer_table_vid.has_value() || !peer_dice_vid.has_value() || !peer_prize_vid.has_value()) {
        return false;
    }

    const std::optional<VID> coordinator_table_vid =
        ResolveCoordinatorVidForPeerVid(coordinator, peer, *peer_table_vid);
    const std::optional<VID> coordinator_dice_vid =
        ResolveCoordinatorVidForPeerVid(coordinator, peer, *peer_dice_vid);
    const std::optional<VID> coordinator_prize_vid =
        ResolveCoordinatorVidForPeerVid(coordinator, peer, *peer_prize_vid);
    if (!coordinator_table_vid.has_value() ||
        !coordinator_dice_vid.has_value() ||
        !coordinator_prize_vid.has_value()) {
        std::cerr << "network frame smoke failed at " << label
                  << ": could not resolve chance-shop links\n";
        return false;
    }

    if (Entity* const table = coordinator.entity_manager.GetEntityMut(*coordinator_table_vid)) {
        table->entity_b = *coordinator_dice_vid;
        table->entity_c = *coordinator_prize_vid;
        table->size = Vec2::New(128.0F, 32.0F);
        world_ops::PatchEntityState(coordinator, *table, *table);
    }
    if (Entity* const buyer = coordinator.entity_manager.GetEntityMut(coordinator_peer_vid)) {
        buyer->money = 4000;
        world_ops::PatchPlayerState(coordinator, *buyer);
    }
    if (Entity* const buyer = peer.entity_manager.GetEntityMut(peer_player_vid)) {
        buyer->money = 4000;
    }
    if (!DeliverCoordinatorPacketsToPeer(
            coordinator,
            peer,
            CoordinatorSmokeTransport(coordinator),
            PeerSmokeTransport(peer),
            pair.peer_endpoint,
            graphics,
            audio,
            label,
            false
        )) {
        return false;
    }

    if (!MovePeerAndCoordinatorEntityTo(
            coordinator,
            peer,
            peer_player_vid,
            table_pos
        )) {
        std::cerr << "network frame smoke failed at " << label
                  << ": could not move player to chance table\n";
        return false;
    }
    const std::size_t coordinator_prize_count_before =
        CountActiveEntitiesOfType(coordinator, EntityType::JetPack);
    const std::size_t peer_prize_count_before =
        CountActiveEntitiesOfType(peer, EntityType::JetPack);
    const std::optional<network::NetEntityId> peer_table_net_id =
        peer.net_session.FindNetEntityId(*peer_table_vid);
    const std::optional<VID> coordinator_table_from_peer_net_id =
        peer_table_net_id.has_value()
            ? coordinator.net_session.FindLocalVid(*peer_table_net_id)
            : std::nullopt;
    if (!peer_table_net_id.has_value() ||
        !coordinator_table_from_peer_net_id.has_value() ||
        *coordinator_table_from_peer_net_id != *coordinator_table_vid) {
        std::cerr << "network frame smoke failed at " << label
                  << ": chance-shop table net id mapping mismatch\n";
        return false;
    }

    if (!StepPeerActionThroughCoordinatorFrame(
            coordinator,
            peer,
            pair,
            GameplayActionRequested{
                .kind = GameplayActionKind::InteractEntity,
                .source_vid = peer_player_vid,
                .target_vid = *peer_table_vid,
            },
            graphics,
            audio,
            label,
            false
        )) {
        return false;
    }
    const Entity* const coordinator_table_after_interact =
        coordinator.entity_manager.GetEntity(*coordinator_table_vid);
    if (coordinator_table_after_interact == nullptr ||
        coordinator_table_after_interact->counter_a != 1.0F) {
        const Entity* const coordinator_player =
            coordinator.entity_manager.GetEntity(coordinator_peer_vid);
        std::cerr << "network frame smoke failed at " << label
                  << ": chance-shop roll did not start"
                  << " table_counter="
                  << (coordinator_table_after_interact != nullptr
                          ? coordinator_table_after_interact->counter_a
                          : -1.0F)
                  << " player_money="
                  << (coordinator_player != nullptr ? coordinator_player->money : 0)
                  << " player_pos="
                  << (coordinator_player != nullptr ? coordinator_player->pos.x : -1.0F)
                  << ","
                  << (coordinator_player != nullptr ? coordinator_player->pos.y : -1.0F)
                  << " table_pos="
                  << (coordinator_table_after_interact != nullptr
                          ? coordinator_table_after_interact->pos.x
                          : -1.0F)
                  << ","
                  << (coordinator_table_after_interact != nullptr
                          ? coordinator_table_after_interact->pos.y
                          : -1.0F)
                  << " table_link_b="
                  << (coordinator_table_after_interact != nullptr &&
                              coordinator_table_after_interact->entity_b.has_value()
                          ? static_cast<int>(coordinator_table_after_interact->entity_b->id)
                          : -1)
                  << '\n';
        return false;
    }

    if (Entity* const dice = coordinator.entity_manager.GetEntityMut(*coordinator_dice_vid)) {
        dice->counter_a = static_cast<float>(forced_roll);
        dice->counter_b = 0.0F;
        dice->grounded = true;
        dice->vel = Vec2::New(0.0F, 0.0F);
        dice->acc = Vec2::New(0.0F, 0.0F);
        world_ops::PatchEntityState(coordinator, *dice, *dice);
    }
    if (!StepIdleFrameAndCompare(coordinator, peer, pair, graphics, audio, label, false)) {
        return false;
    }

    const Entity* const coordinator_buyer = coordinator.entity_manager.GetEntity(coordinator_peer_vid);
    const Entity* const peer_buyer = peer.entity_manager.GetEntity(peer_player_vid);
    if (coordinator_buyer == nullptr ||
        peer_buyer == nullptr ||
        coordinator_buyer->money != expected_money ||
        peer_buyer->money != expected_money) {
        std::cerr << "network frame smoke failed at " << label
                  << ": chance-shop money mismatch coordinator="
                  << (coordinator_buyer != nullptr ? coordinator_buyer->money : 0)
                  << " peer=" << (peer_buyer != nullptr ? peer_buyer->money : 0)
                  << " expected=" << expected_money << '\n';
        return false;
    }

    const std::size_t expected_prize_delta = expect_prize ? 1U : 0U;
    if (CountActiveEntitiesOfType(coordinator, EntityType::JetPack) !=
            coordinator_prize_count_before + expected_prize_delta ||
        CountActiveEntitiesOfType(peer, EntityType::JetPack) !=
            peer_prize_count_before + expected_prize_delta) {
        std::cerr << "network frame smoke failed at " << label
                  << ": chance-shop prize count mismatch\n";
        return false;
    }
    return true;
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

bool RunBombChainReactionScenario(Graphics& graphics, Audio& audio) {
    State coordinator = State::New();
    State peer = State::New();
    PacketSmokePair pair;
    VID coordinator_host_vid;
    VID coordinator_peer_vid;
    VID peer_host_vid;
    VID peer_player_vid;
    if (!BuildDualPlayerFrameFixture(
            coordinator,
            peer,
            pair,
            coordinator_host_vid,
            coordinator_peer_vid,
            peer_host_vid,
            peer_player_vid,
            graphics
        )) {
        return false;
    }

    const std::optional<VID> peer_first_bomb_vid = SpawnCoordinatorEntityAndResolvePeer(
        coordinator,
        peer,
        pair,
        graphics,
        audio,
        EntityType::Bomb,
        Vec2::New(160.0F, 96.0F),
        "frame setup bomb chain first",
        false
    );
    const std::optional<VID> peer_second_bomb_vid = SpawnCoordinatorEntityAndResolvePeer(
        coordinator,
        peer,
        pair,
        graphics,
        audio,
        EntityType::Bomb,
        Vec2::New(172.0F, 96.0F),
        "frame setup bomb chain second",
        false
    );
    if (!peer_first_bomb_vid.has_value() || !peer_second_bomb_vid.has_value()) {
        return false;
    }
    const std::optional<VID> coordinator_first_bomb_vid =
        ResolveCoordinatorVidForPeerVid(coordinator, peer, *peer_first_bomb_vid);
    if (!coordinator_first_bomb_vid.has_value()) {
        std::cerr << "network frame smoke failed: could not resolve bomb chain first bomb\n";
        return false;
    }
    if (Entity* const first_bomb =
            coordinator.entity_manager.GetEntityMut(*coordinator_first_bomb_vid)) {
        first_bomb->counter_a = 1.0F;
        world_ops::PatchEntityState(coordinator, *first_bomb, *first_bomb);
    }
    if (!DeliverCoordinatorPacketsToPeer(
            coordinator,
            peer,
            CoordinatorSmokeTransport(coordinator),
            PeerSmokeTransport(peer),
            pair.peer_endpoint,
            graphics,
            audio,
            "frame arm bomb chain first",
            false
        )) {
        return false;
    }

    for (int i = 0; i < 4; ++i) {
        if (!StepIdleFrameAndCompare(
                coordinator,
                peer,
                pair,
                graphics,
                audio,
                "frame bomb chain reaction",
                false
            )) {
            return false;
        }
    }
    if (CountActiveEntitiesOfType(coordinator, EntityType::Bomb) != 0 ||
        CountActiveEntitiesOfType(peer, EntityType::Bomb) != 0) {
        std::cerr << "network frame smoke failed: bomb chain did not deactivate both bombs\n";
        return false;
    }
    return true;
}

bool RunArrowPushblockAttachmentScenario(Graphics& graphics, Audio& audio) {
    State coordinator = State::New();
    State peer = State::New();
    PacketSmokePair pair;
    VID coordinator_host_vid;
    VID coordinator_peer_vid;
    VID peer_host_vid;
    VID peer_player_vid;
    if (!BuildDualPlayerFrameFixture(
            coordinator,
            peer,
            pair,
            coordinator_host_vid,
            coordinator_peer_vid,
            peer_host_vid,
            peer_player_vid,
            graphics
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
        Vec2::New(160.0F, 96.0F),
        "frame setup arrow block target",
        false
    );
    const std::optional<VID> peer_arrow_vid = SpawnCoordinatorEntityAndResolvePeer(
        coordinator,
        peer,
        pair,
        graphics,
        audio,
        EntityType::Arrow,
        Vec2::New(128.0F, 100.0F),
        "frame setup arrow block projectile",
        false
    );
    if (!peer_block_vid.has_value() || !peer_arrow_vid.has_value()) {
        return false;
    }
    const std::optional<VID> coordinator_block_vid =
        ResolveCoordinatorVidForPeerVid(coordinator, peer, *peer_block_vid);
    const std::optional<VID> coordinator_arrow_vid =
        ResolveCoordinatorVidForPeerVid(coordinator, peer, *peer_arrow_vid);
    if (!coordinator_block_vid.has_value() || !coordinator_arrow_vid.has_value()) {
        std::cerr << "network frame smoke failed: could not resolve arrow/block ids\n";
        return false;
    }

    if (Entity* const arrow = coordinator.entity_manager.GetEntityMut(*coordinator_arrow_vid)) {
        arrow->vel = Vec2::New(8.0F, 0.0F);
        arrow->acc = Vec2::New(0.0F, 0.0F);
        arrow->projectile_contact_timer = 120;
        arrow->projectile_contact_damage_type = DamageType::Attack;
        arrow->projectile_contact_damage_amount = 2;
        arrow->facing = LeftOrRight::Right;
        world_ops::PatchEntityState(coordinator, *arrow, *arrow);
    }
    if (!DeliverCoordinatorPacketsToPeer(
            coordinator,
            peer,
            CoordinatorSmokeTransport(coordinator),
            PeerSmokeTransport(peer),
            pair.peer_endpoint,
            graphics,
            audio,
            "frame arm arrow block projectile",
            false
        )) {
        return false;
    }

    for (int i = 0; i < 8; ++i) {
        if (!StepIdleFrameAndCompare(
                coordinator,
                peer,
                pair,
                graphics,
                audio,
                "frame arrow block attach",
                false
            )) {
            return false;
        }
    }

    const Entity* coordinator_arrow =
        coordinator.entity_manager.GetEntity(*coordinator_arrow_vid);
    const Entity* peer_arrow = peer.entity_manager.GetEntity(*peer_arrow_vid);
    if (coordinator_arrow == nullptr ||
        peer_arrow == nullptr ||
        coordinator_arrow->entity_a != coordinator_block_vid ||
        peer_arrow->entity_a != peer_block_vid ||
        coordinator_arrow->has_physics ||
        peer_arrow->has_physics) {
        std::cerr << "network frame smoke failed: arrow did not attach to pushblock on both peers\n";
        return false;
    }

    const Vec2 coordinator_arrow_pos_before = coordinator_arrow->pos;
    const Vec2 peer_arrow_pos_before = peer_arrow->pos;
    if (Entity* const block = coordinator.entity_manager.GetEntityMut(*coordinator_block_vid)) {
        block->pos = block->pos + Vec2::New(16.0F, 0.0F);
        world_ops::PatchEntityState(coordinator, *block, *block);
    }
    if (!StepIdleFrameAndCompare(
            coordinator,
            peer,
            pair,
            graphics,
            audio,
            "frame arrow follows moved block",
            false
        )) {
        return false;
    }
    coordinator_arrow = coordinator.entity_manager.GetEntity(*coordinator_arrow_vid);
    peer_arrow = peer.entity_manager.GetEntity(*peer_arrow_vid);
    if (coordinator_arrow == nullptr ||
        peer_arrow == nullptr ||
        coordinator_arrow->pos == coordinator_arrow_pos_before ||
        peer_arrow->pos == peer_arrow_pos_before) {
        std::cerr << "network frame smoke failed: attached arrow did not follow moved pushblock\n";
        return false;
    }
    return true;
}

bool RunArrowTrapFiringScenario(Graphics& graphics, Audio& audio) {
    State coordinator = State::New();
    State peer = State::New();
    PacketSmokePair pair;
    VID coordinator_host_vid;
    VID coordinator_peer_vid;
    VID peer_host_vid;
    VID peer_player_vid;
    if (!BuildDualPlayerFrameFixture(
            coordinator,
            peer,
            pair,
            coordinator_host_vid,
            coordinator_peer_vid,
            peer_host_vid,
            peer_player_vid,
            graphics
        )) {
        return false;
    }

    const std::optional<VID> peer_trap_vid = SpawnCoordinatorEntityAndResolvePeer(
        coordinator,
        peer,
        pair,
        graphics,
        audio,
        EntityType::ArrowTrap,
        Vec2::New(160.0F, 96.0F),
        "frame setup arrow trap",
        false
    );
    if (!peer_trap_vid.has_value()) {
        return false;
    }
    const std::optional<VID> coordinator_trap_vid =
        ResolveCoordinatorVidForPeerVid(coordinator, peer, *peer_trap_vid);
    if (!coordinator_trap_vid.has_value()) {
        std::cerr << "network frame smoke failed: could not resolve arrow trap id\n";
        return false;
    }

    if (!MovePeerAndCoordinatorEntityTo(
            coordinator,
            peer,
            peer_player_vid,
            Vec2::New(120.0F, 98.0F)
        )) {
        return false;
    }
    if (Entity* const player = coordinator.entity_manager.GetEntityMut(coordinator_peer_vid)) {
        player->vel = Vec2::New(1.0F, 0.0F);
    }
    if (Entity* const player = peer.entity_manager.GetEntityMut(peer_player_vid)) {
        player->vel = Vec2::New(1.0F, 0.0F);
    }

    if (!StepIdleFrameAndCompare(
            coordinator,
            peer,
            pair,
            graphics,
            audio,
            "frame arrow trap fires",
            false
        )) {
        return false;
    }

    const Entity* const coordinator_trap =
        coordinator.entity_manager.GetEntity(*coordinator_trap_vid);
    const Entity* const peer_trap = peer.entity_manager.GetEntity(*peer_trap_vid);
    if (coordinator_trap == nullptr ||
        peer_trap == nullptr ||
        coordinator_trap->counter_a <= 0.0F ||
        peer_trap->counter_a <= 0.0F ||
        CountActiveEntitiesOfType(coordinator, EntityType::Arrow) != 1 ||
        CountActiveEntitiesOfType(peer, EntityType::Arrow) != 1) {
        std::cerr << "network frame smoke failed: arrow trap fire did not converge\n";
        return false;
    }
    return true;
}

bool FluidCellsMatchInRegion(
    const State& coordinator,
    const State& peer,
    IVec2 tl,
    IVec2 br,
    const char* label
) {
    for (int y = tl.y; y <= br.y; ++y) {
        for (int x = tl.x; x <= br.x; ++x) {
            const unsigned int ux = static_cast<unsigned int>(x);
            const unsigned int uy = static_cast<unsigned int>(y);
            const Tile coordinator_tile = coordinator.stage.GetFluidTile(ux, uy);
            const Tile peer_tile = peer.stage.GetFluidTile(ux, uy);
            const float coordinator_amount = coordinator.stage.GetFluidAmount(ux, uy);
            const float peer_amount = peer.stage.GetFluidAmount(ux, uy);
            if (coordinator_tile != peer_tile ||
                std::abs(coordinator_amount - peer_amount) > 0.001F) {
                std::cerr << "network frame smoke failed at " << label
                          << ": fluid cell mismatch at (" << x << ", " << y << ")"
                          << " coordinator_tile=" << static_cast<int>(coordinator_tile)
                          << " peer_tile=" << static_cast<int>(peer_tile)
                          << " coordinator_amount=" << coordinator_amount
                          << " peer_amount=" << peer_amount << '\n';
                return false;
            }
        }
    }
    return true;
}

bool RunFluidPatchConvergenceScenario(Graphics& graphics, Audio& audio) {
    State coordinator = State::New();
    State peer = State::New();
    PacketSmokePair pair;
    VID coordinator_host_vid;
    VID coordinator_peer_vid;
    VID peer_host_vid;
    VID peer_player_vid;
    if (!BuildDualPlayerFrameFixture(
            coordinator,
            peer,
            pair,
            coordinator_host_vid,
            coordinator_peer_vid,
            peer_host_vid,
            peer_player_vid,
            graphics
        )) {
        return false;
    }

    coordinator.settings.fluid.simulation_enabled = true;
    coordinator.settings.fluid.simulation_interval_frames = 1;
    coordinator.settings.fluid.transfer_per_step = 1.0F;
    coordinator.settings.fluid.gravity_x = 0.0F;
    coordinator.settings.fluid.gravity_y = 1.0F;
    peer.settings.fluid = coordinator.settings.fluid;

    for (int y = 5; y <= 10; ++y) {
        for (int x = 7; x <= 10; ++x) {
            coordinator.stage.SetTile(IVec2::New(x, y), Tile::Air);
            peer.stage.SetTile(IVec2::New(x, y), Tile::Air);
            coordinator.stage.SetFluidTile(IVec2::New(x, y), Tile::Air);
            peer.stage.SetFluidTile(IVec2::New(x, y), Tile::Air);
        }
    }

    coordinator.stage.SetFluidTile(IVec2::New(8, 5), Tile::WaterSwim);
    coordinator.stage.fluid_amount[5][8] = 1.0F;
    coordinator.stage.fluid_display_amount[5][8] = 1.0F;
    coordinator.stage.fluid_velocity[5][8] = Vec2::New(0.0F, 0.0F);
    if (!DeliverCoordinatorPacketsToPeer(
            coordinator,
            peer,
            CoordinatorSmokeTransport(coordinator),
            PeerSmokeTransport(peer),
            pair.peer_endpoint,
            graphics,
            audio,
            "frame initial fluid patch",
            false
        )) {
        return false;
    }
    if (!FluidCellsMatchInRegion(
            coordinator,
            peer,
            IVec2::New(7, 5),
            IVec2::New(10, 10),
            "frame initial fluid patch"
        )) {
        return false;
    }

    for (int i = 0; i < 8; ++i) {
        if (!StepIdleFrameAndCompare(
                coordinator,
                peer,
                pair,
                graphics,
                audio,
                "frame fluid patch convergence",
                false
            )) {
            return false;
        }
        if (!FluidCellsMatchInRegion(
                coordinator,
                peer,
                IVec2::New(7, 5),
                IVec2::New(10, 10),
                "frame fluid patch convergence"
            )) {
            return false;
        }
    }
    return true;
}

bool AssertNetworkPlayersActiveNormal(
    const State& state,
    const char* label
);

bool RunAdminHostStageLoadAndEntitySpawnScenario(Graphics& graphics, Audio& audio) {
    State coordinator = State::New();
    State peer = State::New();
    PacketSmokePair pair;
    VID coordinator_host_vid;
    VID coordinator_peer_vid;
    VID peer_host_vid;
    VID peer_player_vid;
    if (!BuildDualPlayerFrameFixture(
            coordinator,
            peer,
            pair,
            coordinator_host_vid,
            coordinator_peer_vid,
            peer_host_vid,
            peer_player_vid,
            graphics
        )) {
        return false;
    }

    const std::optional<VID> peer_jetpack_vid = SpawnCoordinatorEntityAndResolvePeer(
        coordinator,
        peer,
        pair,
        graphics,
        audio,
        EntityType::JetPack,
        Vec2::New(144.0F, 96.0F),
        "frame admin host entity spawn",
        false
    );
    if (!peer_jetpack_vid.has_value()) {
        return false;
    }
    const Entity* const peer_jetpack = peer.entity_manager.GetEntity(*peer_jetpack_vid);
    if (peer_jetpack == nullptr ||
        !peer_jetpack->active ||
        peer_jetpack->type_ != EntityType::JetPack) {
        std::cerr << "network frame smoke failed: host admin entity spawn did not reach peer\n";
        return false;
    }

    Entity* const coordinator_admin_player = coordinator.entity_manager.GetEntityMut(coordinator_host_vid);
    if (coordinator_admin_player == nullptr) {
        std::cerr << "network frame smoke failed: missing host admin edit player\n";
        return false;
    }
    coordinator_admin_player->money = 54321;
    coordinator_admin_player->wanted = true;
    coordinator_admin_player->pushable = true;
    (void)AddEffect(*coordinator_admin_player, EffectId::Gloves);
    FillToolSlot(
        coordinator.entity_tools.EnsureToolSlot(coordinator_admin_player->vid, 0),
        ToolKind::ThrowStickyBomb,
        77,
        true
    );
    world_ops::PatchEntityState(coordinator, *coordinator_admin_player, *coordinator_admin_player);
    world_ops::PatchPlayerState(coordinator, *coordinator_admin_player);
    if (!DeliverCoordinatorPacketsToPeer(
            coordinator,
            peer,
            CoordinatorSmokeTransport(coordinator),
            PeerSmokeTransport(peer),
            pair.peer_endpoint,
            graphics,
            audio,
            "frame admin host entity edit",
            false
        )) {
        return false;
    }

    const Entity* const peer_admin_player = peer.entity_manager.GetEntity(peer_host_vid);
    const ToolSlot* const peer_admin_tool = peer.entity_tools.FindToolSlot(peer_host_vid, 0);
    if (peer_admin_player == nullptr ||
        peer_admin_player->money != 54321 ||
        !peer_admin_player->wanted ||
        !peer_admin_player->pushable ||
        !HasEffect(*peer_admin_player, EffectId::Gloves) ||
        peer_admin_tool == nullptr ||
        !peer_admin_tool->active ||
        peer_admin_tool->kind != ToolKind::ThrowStickyBomb ||
        peer_admin_tool->count != 77) {
        std::cerr << "network frame smoke failed: host admin entity/effect/tool/money edit did not converge\n";
        return false;
    }

    const IVec2 kAdminTilePos = IVec2::New(11, 9);
    if (!world_ops::SetForegroundTile(coordinator, kAdminTilePos, Tile::Spikes, kTileRotation90)) {
        std::cerr << "network frame smoke failed: host admin tile brush edit failed locally\n";
        return false;
    }
    if (!DeliverCoordinatorPacketsToPeer(
            coordinator,
            peer,
            CoordinatorSmokeTransport(coordinator),
            PeerSmokeTransport(peer),
            pair.peer_endpoint,
            graphics,
            audio,
            "frame admin host tile brush",
            false
        )) {
        return false;
    }
    if (peer.stage.GetTile(
            static_cast<unsigned int>(kAdminTilePos.x),
            static_cast<unsigned int>(kAdminTilePos.y)
        ) != Tile::Spikes ||
        peer.stage.GetTileRotation(
            static_cast<unsigned int>(kAdminTilePos.x),
            static_cast<unsigned int>(kAdminTilePos.y)
        ) != kTileRotation90) {
        std::cerr << "network frame smoke failed: host admin tile brush edit did not converge\n";
        return false;
    }

    constexpr std::uint32_t kAdminStageSeed = 24680;
    if (!LoadQuestStage(coordinator, "classic", "classic_mines_2", true, kAdminStageSeed)) {
        std::cerr << "network frame smoke failed: host admin stage load failed locally\n";
        return false;
    }
    std::string status;
    (void)network::ReviveNetworkPlayersAtEntrance(coordinator, graphics, &status);
    network::NotifyStageLoaded(coordinator);

    for (int i = 0; i < 80; ++i) {
        StepSingleTick(peer, audio, graphics);
        StepSingleTick(coordinator, audio, graphics);
        if (!DeliverCoordinatorPacketsToPeer(
                coordinator,
                peer,
                CoordinatorSmokeTransport(coordinator),
                PeerSmokeTransport(peer),
                pair.peer_endpoint,
                graphics,
                audio,
                "frame admin host stage load",
                false
            )) {
            return false;
        }
        if (peer.stage.quest_stage_id == "classic_mines_2" &&
            peer.mode == Mode::Playing) {
            break;
        }
    }

    if (coordinator.stage.quest_stage_id != "classic_mines_2" ||
        peer.stage.quest_stage_id != "classic_mines_2" ||
        coordinator.net_session.stage_instance_id != peer.net_session.stage_instance_id ||
        coordinator.stage.generation_seed.value_or(0) != kAdminStageSeed ||
        peer.stage.generation_seed.value_or(0) != kAdminStageSeed ||
        peer.mode != Mode::Playing) {
        std::cerr << "network frame smoke failed: host admin stage load did not converge\n";
        return false;
    }
    return AssertNetworkPlayersActiveNormal(coordinator, "frame admin stage load coordinator") &&
           AssertNetworkPlayersActiveNormal(peer, "frame admin stage load peer");
}

bool StepStageTransitionFramesAndCompare(
    State& coordinator,
    State& peer,
    PacketSmokePair& pair,
    Graphics& graphics,
    Audio& audio,
    bool drop_first_coordinator_delivery = false
) {
    constexpr int kNormalTransitionFrames = 64;
    constexpr int kImpairedTransitionFrames = 140;
    const int transition_frames = drop_first_coordinator_delivery
        ? kImpairedTransitionFrames
        : kNormalTransitionFrames;
    bool dropped_coordinator_delivery = false;
    for (int i = 0; i < transition_frames; ++i) {
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
        if (drop_first_coordinator_delivery && !dropped_coordinator_delivery) {
            if (!DropCoordinatorPacketsToPeer(
                    coordinator,
                    CoordinatorSmokeTransport(coordinator),
                    "frame stage transition dropped coordinator delivery"
                )) {
                return false;
            }
            dropped_coordinator_delivery = true;
            continue;
        }
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
        if (coordinator.mode == Mode::Playing &&
            peer.mode == Mode::Playing &&
            coordinator.net_session.stage_instance_id == peer.net_session.stage_instance_id &&
            coordinator.stage.quest_id == peer.stage.quest_id &&
            coordinator.stage.quest_stage_id == peer.stage.quest_stage_id) {
            break;
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

bool AssertNetworkPlayersActiveNormal(
    const State& state,
    const char* label
) {
    for (const PlayerSlot& slot : state.players.slots) {
        if (!slot.connected || !slot.entity_vid.has_value()) {
            continue;
        }
        const Entity* const entity = state.entity_manager.GetEntity(*slot.entity_vid);
        if (entity == nullptr ||
            !entity->active ||
            entity->condition != EntityCondition::Normal ||
            entity->health == 0 ||
            entity->held_by_vid.has_value() ||
            entity->holding_vid.has_value() ||
            entity->attachment_mode != AttachmentMode::None) {
            std::cerr << "network frame smoke failed at " << label
                      << ": player " << slot.player_id << " bad lifecycle state\n";
            return false;
        }
    }
    return true;
}

bool BuildDualPlayerFrameFixture(
    State& coordinator,
    State& peer,
    PacketSmokePair& pair,
    VID& coordinator_host_vid,
    VID& coordinator_peer_vid,
    VID& peer_host_vid,
    VID& peer_player_vid,
    Graphics& graphics
) {
    constexpr std::uint32_t seed = 54321;
    coordinator = State::New();
    peer = State::New();
    ConfigureProtocolSmokeCoordinator(coordinator);
    ConfigureProtocolSmokePeer(peer);
    if (!LoadQuestStage(coordinator, "classic", "classic_mines_1", false, seed) ||
        !LoadQuestStage(peer, "classic", "classic_mines_1", false, seed)) {
        std::cerr << "network frame smoke failed: could not load dual-player stages\n";
        return false;
    }
    ConfigureProtocolSmokeCoordinator(coordinator);
    ConfigureProtocolSmokePeer(peer);
    pair = MakePacketSmokePair();
    AttachPacketSmokeTransports(coordinator, peer, pair);
    return ConfigureDualPlayerFrameSmoke(
        coordinator,
        peer,
        graphics,
        coordinator_host_vid,
        coordinator_peer_vid,
        peer_host_vid,
        peer_player_vid
    );
}

bool RunStageTransitionWithDeadHostScenario(Graphics& graphics, Audio& audio) {
    State coordinator = State::New();
    State peer = State::New();
    PacketSmokePair pair;
    VID coordinator_host_vid;
    VID coordinator_peer_vid;
    VID peer_host_vid;
    VID peer_player_vid;
    if (!BuildDualPlayerFrameFixture(
            coordinator,
            peer,
            pair,
            coordinator_host_vid,
            coordinator_peer_vid,
            peer_host_vid,
            peer_player_vid,
            graphics
        )) {
        return false;
    }

    for (const VID vid : {coordinator_host_vid, peer_host_vid}) {
        State& state = vid == coordinator_host_vid ? coordinator : peer;
        if (Entity* const host = state.entity_manager.GetEntityMut(vid)) {
            host->health = 0;
            host->condition = EntityCondition::Dead;
            host->vel = Vec2::New(0.0F, 0.0F);
            host->acc = Vec2::New(0.0F, 0.0F);
        }
    }

    const Entity* const peer_exit = FindFirstEntityOfType(peer, EntityType::BasicExit);
    if (peer_exit == nullptr) {
        std::cerr << "network frame smoke failed: missing peer exit for dead-host transition\n";
        return false;
    }
    if (!MovePeerAndCoordinatorEntityTo(coordinator, peer, peer_player_vid, peer_exit->pos)) {
        std::cerr << "network frame smoke failed: could not move peer to exit for dead-host transition\n";
        return false;
    }
    if (!StepPeerActionThroughCoordinatorFrame(
            coordinator,
            peer,
            pair,
            GameplayActionRequested{
                .kind = GameplayActionKind::InteractEntity,
                .source_vid = peer_player_vid,
                .target_vid = peer_exit->vid,
            },
            graphics,
            audio,
            "frame dead host exit interact",
            false
        )) {
        return false;
    }
    if (!StepStageTransitionFramesAndCompare(coordinator, peer, pair, graphics, audio)) {
        return false;
    }
    return AssertNetworkPlayersActiveNormal(coordinator, "frame dead host transition coordinator") &&
           AssertNetworkPlayersActiveNormal(peer, "frame dead host transition peer");
}

bool RunStageTransitionWhileCarryingPlayerScenario(Graphics& graphics, Audio& audio) {
    State coordinator = State::New();
    State peer = State::New();
    PacketSmokePair pair;
    VID coordinator_host_vid;
    VID coordinator_peer_vid;
    VID peer_host_vid;
    VID peer_player_vid;
    if (!BuildDualPlayerFrameFixture(
            coordinator,
            peer,
            pair,
            coordinator_host_vid,
            coordinator_peer_vid,
            peer_host_vid,
            peer_player_vid,
            graphics
        )) {
        return false;
    }

    if (!StepPeerActionThroughCoordinatorFrame(
            coordinator,
            peer,
            pair,
            GameplayActionRequested{
                .kind = GameplayActionKind::PickupEntity,
                .source_vid = peer_player_vid,
                .target_vid = peer_host_vid,
            },
            graphics,
            audio,
            "frame pickup host player before transition",
            false
        )) {
        return false;
    }

    const Entity* const peer_exit = FindFirstEntityOfType(peer, EntityType::BasicExit);
    if (peer_exit == nullptr) {
        std::cerr << "network frame smoke failed: missing peer exit for carry transition\n";
        return false;
    }
    if (!MovePeerAndCoordinatorEntityTo(coordinator, peer, peer_player_vid, peer_exit->pos)) {
        std::cerr << "network frame smoke failed: could not move peer to exit for carry transition\n";
        return false;
    }
    if (!StepPeerActionThroughCoordinatorFrame(
            coordinator,
            peer,
            pair,
            GameplayActionRequested{
                .kind = GameplayActionKind::InteractEntity,
                .source_vid = peer_player_vid,
                .target_vid = peer_exit->vid,
            },
            graphics,
            audio,
            "frame carrying player exit interact",
            false
        )) {
        return false;
    }
    if (!StepStageTransitionFramesAndCompare(coordinator, peer, pair, graphics, audio)) {
        return false;
    }
    return AssertNetworkPlayersActiveNormal(coordinator, "frame carry transition coordinator") &&
           AssertNetworkPlayersActiveNormal(peer, "frame carry transition peer");
}

bool RunStageTransitionDroppedInitialSyncScenario(Graphics& graphics, Audio& audio) {
    State coordinator = State::New();
    State peer = State::New();
    PacketSmokePair pair;
    VID coordinator_host_vid;
    VID coordinator_peer_vid;
    VID peer_host_vid;
    VID peer_player_vid;
    if (!BuildDualPlayerFrameFixture(
            coordinator,
            peer,
            pair,
            coordinator_host_vid,
            coordinator_peer_vid,
            peer_host_vid,
            peer_player_vid,
            graphics
        )) {
        return false;
    }

    const Entity* const peer_exit = FindFirstEntityOfType(peer, EntityType::BasicExit);
    if (peer_exit == nullptr) {
        std::cerr << "network frame smoke failed: missing peer exit for dropped-sync transition\n";
        return false;
    }
    if (!MovePeerAndCoordinatorEntityTo(coordinator, peer, peer_player_vid, peer_exit->pos)) {
        std::cerr << "network frame smoke failed: could not move peer to exit for dropped-sync transition\n";
        return false;
    }
    if (!StepPeerActionThroughCoordinatorFrame(
            coordinator,
            peer,
            pair,
            GameplayActionRequested{
                .kind = GameplayActionKind::InteractEntity,
                .source_vid = peer_player_vid,
                .target_vid = peer_exit->vid,
            },
            graphics,
            audio,
            "frame dropped stage sync exit interact",
            false
        )) {
        return false;
    }
    if (!StepStageTransitionFramesAndCompare(
            coordinator,
            peer,
            pair,
            graphics,
            audio,
            true
        )) {
        return false;
    }
    return AssertNetworkPlayersActiveNormal(coordinator, "frame dropped stage sync coordinator") &&
           AssertNetworkPlayersActiveNormal(peer, "frame dropped stage sync peer");
}

bool AssertCarryLinksSevered(
    const State& state,
    VID holder_vid,
    VID held_vid,
    const char* label
) {
    const Entity* const holder = state.entity_manager.GetEntity(holder_vid);
    const Entity* const held = state.entity_manager.GetEntity(held_vid);
    if (holder == nullptr || held == nullptr) {
        std::cerr << "network frame smoke failed at " << label
                  << ": missing holder or held player\n";
        return false;
    }
    if (holder->holding_vid.has_value() ||
        holder->holding ||
        held->held_by_vid.has_value() ||
        held->attachment_mode != AttachmentMode::None) {
        std::cerr << "network frame smoke failed at " << label
                  << ": carry link survived damage/drop\n";
        return false;
    }
    return true;
}

bool RunDamageWhileCarryingPlayerScenario(Graphics& graphics, Audio& audio) {
    State coordinator = State::New();
    State peer = State::New();
    PacketSmokePair pair;
    VID coordinator_host_vid;
    VID coordinator_peer_vid;
    VID peer_host_vid;
    VID peer_player_vid;
    if (!BuildDualPlayerFrameFixture(
            coordinator,
            peer,
            pair,
            coordinator_host_vid,
            coordinator_peer_vid,
            peer_host_vid,
            peer_player_vid,
            graphics
        )) {
        return false;
    }

    if (!StepPeerActionThroughCoordinatorFrame(
            coordinator,
            peer,
            pair,
            GameplayActionRequested{
                .kind = GameplayActionKind::PickupEntity,
                .source_vid = peer_player_vid,
                .target_vid = peer_host_vid,
            },
            graphics,
            audio,
            "frame pickup host player before holder damage",
            false
        )) {
        return false;
    }

    if (!StepPeerActionThroughCoordinatorFrame(
            coordinator,
            peer,
            pair,
            GameplayActionRequested{
                .kind = GameplayActionKind::DamageEntity,
                .source_vid = peer_player_vid,
                .target_vid = peer_player_vid,
                .damage_type = DamageType::Fall,
                .amount = 1,
            },
            graphics,
            audio,
            "frame holder damaged while carrying player",
            false
        )) {
        return false;
    }

    const Entity* const coordinator_holder = coordinator.entity_manager.GetEntity(coordinator_peer_vid);
    const Entity* const peer_holder = peer.entity_manager.GetEntity(peer_player_vid);
    if (coordinator_holder == nullptr || peer_holder == nullptr ||
        coordinator_holder->condition != EntityCondition::Stunned ||
        peer_holder->condition != EntityCondition::Stunned ||
        coordinator_holder->health != 399 ||
        peer_holder->health != 399) {
        std::cerr << "network frame smoke failed at frame holder damaged while carrying player:"
                  << " holder damage/stun did not converge\n";
        return false;
    }

    return AssertCarryLinksSevered(
               coordinator,
               coordinator_peer_vid,
               coordinator_host_vid,
               "frame holder damage coordinator"
           ) &&
           AssertCarryLinksSevered(
               peer,
               peer_player_vid,
               peer_host_vid,
               "frame holder damage peer"
           );
}

bool RunDeathWhileHeldPlayerScenario(Graphics& graphics, Audio& audio) {
    State coordinator = State::New();
    State peer = State::New();
    PacketSmokePair pair;
    VID coordinator_host_vid;
    VID coordinator_peer_vid;
    VID peer_host_vid;
    VID peer_player_vid;
    if (!BuildDualPlayerFrameFixture(
            coordinator,
            peer,
            pair,
            coordinator_host_vid,
            coordinator_peer_vid,
            peer_host_vid,
            peer_player_vid,
            graphics
        )) {
        return false;
    }

    if (!StepPeerActionThroughCoordinatorFrame(
            coordinator,
            peer,
            pair,
            GameplayActionRequested{
                .kind = GameplayActionKind::PickupEntity,
                .source_vid = peer_player_vid,
                .target_vid = peer_host_vid,
            },
            graphics,
            audio,
            "frame pickup host player before held death",
            false
        )) {
        return false;
    }

    if (!StepPeerActionThroughCoordinatorFrame(
            coordinator,
            peer,
            pair,
            GameplayActionRequested{
                .kind = GameplayActionKind::DamageEntity,
                .source_vid = std::nullopt,
                .target_vid = peer_host_vid,
                .damage_type = DamageType::Fall,
                .amount = 400,
            },
            graphics,
            audio,
            "frame held player killed",
            false
        )) {
        return false;
    }

    const Entity* const coordinator_held = coordinator.entity_manager.GetEntity(coordinator_host_vid);
    const Entity* const peer_held = peer.entity_manager.GetEntity(peer_host_vid);
    if (coordinator_held == nullptr || peer_held == nullptr ||
        coordinator_held->condition != EntityCondition::Dead ||
        peer_held->condition != EntityCondition::Dead ||
        coordinator_held->health != 0 ||
        peer_held->health != 0) {
        std::cerr << "network frame smoke failed: held player death did not converge"
                  << " coordinator_condition="
                  << (coordinator_held != nullptr ? static_cast<int>(coordinator_held->condition) : -1)
                  << " peer_condition="
                  << (peer_held != nullptr ? static_cast<int>(peer_held->condition) : -1)
                  << " coordinator_health="
                  << (coordinator_held != nullptr ? coordinator_held->health : 999999U)
                  << " peer_health="
                  << (peer_held != nullptr ? peer_held->health : 999999U)
                  << '\n';
        return false;
    }
    if (!AssertCarryLinksSevered(
            coordinator,
            coordinator_peer_vid,
            coordinator_host_vid,
            "frame held death coordinator"
        ) ||
        !AssertCarryLinksSevered(
            peer,
            peer_player_vid,
            peer_host_vid,
            "frame held death peer"
        )) {
        return false;
    }

    std::string respawn_status;
    if (!network::RespawnLocalPlayersAtEntrance(coordinator, graphics, &respawn_status)) {
        std::cerr << "network frame smoke failed: " << respawn_status << '\n';
        return false;
    }
    if (!DeliverCoordinatorPacketsToPeer(
            coordinator,
            peer,
            CoordinatorSmokeTransport(coordinator),
            PeerSmokeTransport(peer),
            pair.peer_endpoint,
            graphics,
            audio,
            "frame held player death respawn",
            false
        )) {
        return false;
    }
    return AssertNetworkPlayersActiveNormal(coordinator, "frame held death respawn coordinator") &&
           AssertNetworkPlayersActiveNormal(peer, "frame held death respawn peer");
}

bool RunFallDamageLatencyRepairScenario(Graphics& graphics, Audio& audio) {
    State coordinator = State::New();
    State peer = State::New();
    PacketSmokePair pair;
    VID coordinator_host_vid;
    VID coordinator_peer_vid;
    VID peer_host_vid;
    VID peer_player_vid;
    if (!BuildDualPlayerFrameFixture(
            coordinator,
            peer,
            pair,
            coordinator_host_vid,
            coordinator_peer_vid,
            peer_host_vid,
            peer_player_vid,
            graphics
        )) {
        return false;
    }
    (void)coordinator_host_vid;
    (void)peer_host_vid;

    world_ops::QueuePendingGameplayAction(
        peer,
        GameplayActionRequested{
            .kind = GameplayActionKind::DamageEntity,
            .source_vid = std::nullopt,
            .target_vid = peer_player_vid,
            .damage_type = DamageType::Fall,
            .amount = 1,
        }
    );
    StepSingleTick(peer, audio, graphics);
    if (!DeliverPeerPacketsToCoordinator(
            peer,
            coordinator,
            PeerSmokeTransport(peer),
            CoordinatorSmokeTransport(coordinator),
            pair.peer_endpoint,
            "frame fall damage latency request"
        )) {
        return false;
    }
    StepSingleTick(coordinator, audio, graphics);

    constexpr int kResultLatencyFrames = 4;
    for (int i = 0; i < kResultLatencyFrames; ++i) {
        StepSingleTick(peer, audio, graphics);
        StepSingleTick(coordinator, audio, graphics);
    }

    if (!DeliverCoordinatorPacketsToPeer(
            coordinator,
            peer,
            CoordinatorSmokeTransport(coordinator),
            PeerSmokeTransport(peer),
            pair.peer_endpoint,
            graphics,
            audio,
            "frame fall damage latency repair",
            false
        )) {
        return false;
    }
    if (!DeliverCoordinatorSnapshotsToPeer(
            coordinator,
            peer,
            pair,
            graphics,
            "frame fall damage latency snapshot repair"
        )) {
        return false;
    }
    network::StepRemotePlayerInterpolation(peer, PeerSmokeTransport(peer), graphics);

    const Entity* const coordinator_player =
        coordinator.entity_manager.GetEntity(coordinator_peer_vid);
    const Entity* const peer_player = peer.entity_manager.GetEntity(peer_player_vid);
    if (coordinator_player == nullptr || peer_player == nullptr ||
        coordinator_player->condition != EntityCondition::Stunned ||
        peer_player->condition != EntityCondition::Stunned ||
        coordinator_player->health != 399 ||
        peer_player->health != 399 ||
        coordinator_player->stun_timer != peer_player->stun_timer ||
        coordinator_player->fall_timer != peer_player->fall_timer) {
        std::cerr << "network frame smoke failed at frame fall damage latency repair:"
                  << " coordinator health="
                  << (coordinator_player != nullptr ? coordinator_player->health : 999999U)
                  << " peer health="
                  << (peer_player != nullptr ? peer_player->health : 999999U)
                  << " coordinator condition="
                  << (coordinator_player != nullptr
                          ? static_cast<int>(coordinator_player->condition)
                          : -1)
                  << " peer condition="
                  << (peer_player != nullptr ? static_cast<int>(peer_player->condition) : -1)
                  << " coordinator stun="
                  << (coordinator_player != nullptr ? coordinator_player->stun_timer : 999999U)
                  << " peer stun="
                  << (peer_player != nullptr ? peer_player->stun_timer : 999999U)
                  << " coordinator fall="
                  << (coordinator_player != nullptr ? coordinator_player->fall_timer : 999999U)
                  << " peer fall="
                  << (peer_player != nullptr ? peer_player->fall_timer : 999999U)
                  << '\n';
        return false;
    }

    return true;
}

bool RunPeerActionWithDelayedCoordinatorRepair(
    State& coordinator,
    State& peer,
    PacketSmokePair& pair,
    const GameplayActionRequested& action,
    Graphics& graphics,
    Audio& audio,
    const char* label
) {
    world_ops::QueuePendingGameplayAction(peer, action);
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

    constexpr int kResultLatencyFrames = 4;
    for (int i = 0; i < kResultLatencyFrames; ++i) {
        StepSingleTick(peer, audio, graphics);
        StepSingleTick(coordinator, audio, graphics);
    }

    if (!DeliverCoordinatorPacketsToPeer(
            coordinator,
            peer,
            CoordinatorSmokeTransport(coordinator),
            PeerSmokeTransport(peer),
            pair.peer_endpoint,
            graphics,
            audio,
            label,
            false
        )) {
        return false;
    }
    if (!DeliverCoordinatorSnapshotsToPeer(
            coordinator,
            peer,
            pair,
            graphics,
            label
        )) {
        return false;
    }
    network::StepRemotePlayerInterpolation(peer, PeerSmokeTransport(peer), graphics);
    return true;
}

bool AssertCarryLinkActive(
    const State& state,
    VID holder_vid,
    VID held_vid,
    const char* label
) {
    const Entity* const holder = state.entity_manager.GetEntity(holder_vid);
    const Entity* const held = state.entity_manager.GetEntity(held_vid);
    if (holder == nullptr || held == nullptr) {
        std::cerr << "network frame smoke failed at " << label
                  << ": missing holder or held player\n";
        return false;
    }
    if (holder->holding_vid != held_vid ||
        !holder->holding ||
        held->held_by_vid != holder_vid ||
        held->attachment_mode != AttachmentMode::Held) {
        const long long holder_holding_id = holder->holding_vid.has_value()
            ? static_cast<long long>(holder->holding_vid->id)
            : -1LL;
        const long long held_by_id = held->held_by_vid.has_value()
            ? static_cast<long long>(held->held_by_vid->id)
            : -1LL;
        std::cerr << "network frame smoke failed at " << label
                  << ": carry link not active"
                  << " holder_holding=" << holder_holding_id
                  << " holder_flag=" << holder->holding
                  << " held_by=" << held_by_id
                  << " attachment=" << static_cast<int>(held->attachment_mode)
                  << '\n';
        return false;
    }
    return true;
}

bool RunCarryThrowLatencyRepairScenario(Graphics& graphics, Audio& audio) {
    State coordinator = State::New();
    State peer = State::New();
    PacketSmokePair pair;
    VID coordinator_host_vid;
    VID coordinator_peer_vid;
    VID peer_host_vid;
    VID peer_player_vid;
    if (!BuildDualPlayerFrameFixture(
            coordinator,
            peer,
            pair,
            coordinator_host_vid,
            coordinator_peer_vid,
            peer_host_vid,
            peer_player_vid,
            graphics
        )) {
        return false;
    }

    if (!RunPeerActionWithDelayedCoordinatorRepair(
            coordinator,
            peer,
            pair,
            GameplayActionRequested{
                .kind = GameplayActionKind::PickupEntity,
                .source_vid = peer_player_vid,
                .target_vid = peer_host_vid,
            },
            graphics,
            audio,
            "frame carry latency pickup"
        )) {
        return false;
    }
    if (!AssertCarryLinkActive(
            coordinator,
            coordinator_peer_vid,
            coordinator_host_vid,
            "frame carry latency coordinator"
        ) ||
        !AssertCarryLinkActive(
            peer,
            peer_player_vid,
            peer_host_vid,
            "frame carry latency peer"
        )) {
        return false;
    }

    const Vec2 throw_velocity = Vec2::New(2.25F, -2.0F);
    if (!RunPeerActionWithDelayedCoordinatorRepair(
            coordinator,
            peer,
            pair,
            GameplayActionRequested{
                .kind = GameplayActionKind::ThrowEntity,
                .source_vid = peer_player_vid,
                .target_vid = peer_host_vid,
                .velocity = throw_velocity,
            },
            graphics,
            audio,
            "frame carry latency throw"
        )) {
        return false;
    }
    if (!AssertCarryLinksSevered(
            coordinator,
            coordinator_peer_vid,
            coordinator_host_vid,
            "frame carry latency throw coordinator"
        ) ||
        !AssertCarryLinksSevered(
            peer,
            peer_player_vid,
            peer_host_vid,
            "frame carry latency throw peer"
        )) {
        return false;
    }

    const Entity* const coordinator_thrown =
        coordinator.entity_manager.GetEntity(coordinator_host_vid);
    const Entity* peer_thrown = peer.entity_manager.GetEntity(peer_host_vid);
    if (coordinator_thrown == nullptr || peer_thrown == nullptr ||
        std::fabs(coordinator_thrown->vel.x - peer_thrown->vel.x) > 0.01F ||
        std::fabs(coordinator_thrown->vel.y - peer_thrown->vel.y) > 0.01F) {
        std::cerr << "network frame smoke failed at frame carry latency throw:"
                  << " thrown velocity did not converge"
                  << " coordinator="
                  << (coordinator_thrown != nullptr ? coordinator_thrown->vel.x : 9999.0F)
                  << ","
                  << (coordinator_thrown != nullptr ? coordinator_thrown->vel.y : 9999.0F)
                  << " peer="
                  << (peer_thrown != nullptr ? peer_thrown->vel.x : 9999.0F)
                  << ","
                  << (peer_thrown != nullptr ? peer_thrown->vel.y : 9999.0F)
                  << '\n';
        return false;
    }

    Entity* const peer_thrown_mut = peer.entity_manager.GetEntityMut(peer_host_vid);
    if (peer_thrown_mut == nullptr) {
        std::cerr << "network frame smoke failed at frame carry latency throw:"
                  << " missing mutable peer thrown player\n";
        return false;
    }
    peer_thrown_mut->thrown_by.reset();
    peer_thrown_mut->can_apply_projectile_contact = false;
    peer_thrown_mut->projectile_contact_damage_type = DamageType::Crush;
    peer_thrown_mut->projectile_contact_damage_amount = 0;
    if (!DeliverCoordinatorSnapshotsToPeer(
            coordinator,
            peer,
            pair,
            graphics,
            "frame carry latency projectile contact repair"
        )) {
        return false;
    }
    network::StepRemotePlayerInterpolation(peer, PeerSmokeTransport(peer), graphics);

    peer_thrown = peer.entity_manager.GetEntity(peer_host_vid);
    const std::optional<VID> expected_peer_thrower = peer_player_vid;
    if (peer_thrown == nullptr ||
        peer_thrown->thrown_by != expected_peer_thrower ||
        peer_thrown->can_apply_projectile_contact !=
            coordinator_thrown->can_apply_projectile_contact ||
        peer_thrown->projectile_contact_damage_type !=
            coordinator_thrown->projectile_contact_damage_type ||
        peer_thrown->projectile_contact_damage_amount !=
            coordinator_thrown->projectile_contact_damage_amount) {
        const long long thrown_by_id =
            peer_thrown != nullptr && peer_thrown->thrown_by.has_value()
                ? static_cast<long long>(peer_thrown->thrown_by->id)
                : -1LL;
        std::cerr << "network frame smoke failed at frame carry latency throw:"
                  << " projectile contact body repair did not converge"
                  << " peer_thrown_by=" << thrown_by_id
                  << " peer_can_contact="
                  << (peer_thrown != nullptr ? peer_thrown->can_apply_projectile_contact : false)
                  << " peer_contact_amount="
                  << (peer_thrown != nullptr ? peer_thrown->projectile_contact_damage_amount : 999999U)
                  << '\n';
        return false;
    }
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
        if (!RunBasicMovementLatencySmoke(graphics, audio)) {
            return false;
        }
        if (!RunPlayerCorrectionPolicySmoke(graphics)) {
            return false;
        }
        if (!RunPlayerMovementStateRepairSmoke(graphics, audio)) {
            return false;
        }
        if (!RunPlayerBodyLossRecoverySmoke(graphics)) {
            return false;
        }
        if (!RunStageTransitionWithDeadHostScenario(graphics, audio)) {
            return false;
        }
        if (!RunStageTransitionWhileCarryingPlayerScenario(graphics, audio)) {
            return false;
        }
        if (!RunStageTransitionDroppedInitialSyncScenario(graphics, audio)) {
            return false;
        }
        if (!RunDamageWhileCarryingPlayerScenario(graphics, audio)) {
            return false;
        }
        if (!RunDeathWhileHeldPlayerScenario(graphics, audio)) {
            return false;
        }
        if (!RunFallDamageLatencyRepairScenario(graphics, audio)) {
            return false;
        }
        if (!RunCarryThrowLatencyRepairScenario(graphics, audio)) {
            return false;
        }
        if (!RunBombChainReactionScenario(graphics, audio)) {
            return false;
        }
        if (!RunArrowPushblockAttachmentScenario(graphics, audio)) {
            return false;
        }
        if (!RunArrowTrapFiringScenario(graphics, audio)) {
            return false;
        }
        if (!RunFluidPatchConvergenceScenario(graphics, audio)) {
            return false;
        }
        if (!RunAdminHostStageLoadAndEntitySpawnScenario(graphics, audio)) {
            return false;
        }
        if (!RunChanceShopRollScenario(
                graphics,
                audio,
                8,
                5000,
                false,
                "frame chance shop money win from peer"
            ) ||
            !RunChanceShopRollScenario(
                graphics,
                audio,
                5,
                3000,
                false,
                "frame chance shop loss from peer"
            ) ||
            !RunChanceShopRollScenario(
                graphics,
                audio,
                7,
                3000,
                true,
                "frame chance shop prize win from peer"
            )) {
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

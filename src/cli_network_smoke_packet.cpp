#include "cli_network_smoke.hpp"

#include "cli_network_smoke_internal.hpp"
#include "network/net_lobby_internal.hpp"
#include "network/net_world_snapshot.hpp"
#include "quest_stage_loader.hpp"

#include <algorithm>
#include <exception>
#include <cmath>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace splonks {

namespace {

std::string DescribeNetworkSnapshotDifference(const State& coordinator, const State& peer);

bool CheckSnapshotFingerprint(const State& peer, const char* label) {
    if (peer.net_session.last_snapshot_fingerprint_valid) {
        return true;
    }
    std::cerr << "network packet smoke failed: " << label
              << " snapshot fingerprint mismatch expected="
              << peer.net_session.last_snapshot_expected_fingerprint
              << " actual=" << peer.net_session.last_snapshot_actual_fingerprint
              << '\n';
    return false;
}

bool CheckSnapshotFingerprint(const State& coordinator, const State& peer, const char* label) {
    if (CheckSnapshotFingerprint(peer, label)) {
        return true;
    }
    std::cerr << "  first network snapshot diff: "
              << DescribeNetworkSnapshotDifference(coordinator, peer) << '\n';
    return false;
}

network::NetEntityId SmokeNetIdForVid(const State& state, VID vid) {
    return state.net_session.FindNetEntityId(vid).value_or(network::kInvalidNetEntityId);
}

std::string DescribeNetworkSnapshotDifference(const State& coordinator, const State& peer) {
    if (coordinator.frame != peer.frame ||
        coordinator.stage_frame != peer.stage_frame ||
        coordinator.depth != peer.depth ||
        coordinator.points != peer.points ||
        coordinator.deaths != peer.deaths ||
        coordinator.sac_altar_favor != peer.sac_altar_favor ||
        coordinator.sac_altar_reward_tier != peer.sac_altar_reward_tier ||
        coordinator.game_over != peer.game_over ||
        coordinator.win != peer.win) {
        std::ostringstream output;
        output << "run frame differs c=" << coordinator.frame << "/" << coordinator.stage_frame
               << "/" << coordinator.depth << "/" << coordinator.points << "/"
               << coordinator.deaths << "/" << coordinator.sac_altar_favor << "/"
               << coordinator.sac_altar_reward_tier << "/" << coordinator.game_over << "/"
               << coordinator.win
               << " p=" << peer.frame << "/" << peer.stage_frame << "/" << peer.depth << "/"
               << peer.points << "/" << peer.deaths << "/" << peer.sac_altar_favor << "/"
               << peer.sac_altar_reward_tier << "/" << peer.game_over << "/" << peer.win;
        return output.str();
    }
    if (coordinator.quest_state.quest_id != peer.quest_state.quest_id ||
        coordinator.quest_state.classic.made_black_market != peer.quest_state.classic.made_black_market ||
        coordinator.quest_state.classic.made_udjat_eye != peer.quest_state.classic.made_udjat_eye ||
        coordinator.quest_state.classic.has_udjat_eye != peer.quest_state.classic.has_udjat_eye ||
        coordinator.quest_state.classic.made_moai != peer.quest_state.classic.made_moai ||
        coordinator.quest_state.classic.has_hedjet != peer.quest_state.classic.has_hedjet ||
        coordinator.quest_state.classic.has_sceptre != peer.quest_state.classic.has_sceptre ||
        coordinator.quest_state.classic.has_book_of_dead != peer.quest_state.classic.has_book_of_dead) {
        return "quest state differs";
    }
    if (coordinator.stage.GetStageDims() != peer.stage.GetStageDims()) {
        std::ostringstream output;
        output << "stage dims differ c=" << coordinator.stage.GetTileWidth() << "x"
               << coordinator.stage.GetTileHeight()
               << " p=" << peer.stage.GetTileWidth() << "x" << peer.stage.GetTileHeight();
        return output.str();
    }
    if (coordinator.stage.quest_id != peer.stage.quest_id ||
        coordinator.stage.quest_stage_id != peer.stage.quest_stage_id ||
        coordinator.stage.route_label != peer.stage.route_label ||
        coordinator.stage.quest_level_number != peer.stage.quest_level_number ||
        coordinator.stage.generation_seed != peer.stage.generation_seed ||
        coordinator.stage.stage_type != peer.stage.stage_type ||
        coordinator.stage.gravity != peer.stage.gravity ||
        coordinator.stage.border.left.tile != peer.stage.border.left.tile ||
        coordinator.stage.border.right.tile != peer.stage.border.right.tile ||
        coordinator.stage.border.top.tile != peer.stage.border.top.tile ||
        coordinator.stage.border.bottom.tile != peer.stage.border.bottom.tile ||
        coordinator.stage.border.wrap_x != peer.stage.border.wrap_x ||
        coordinator.stage.border.wrap_y != peer.stage.border.wrap_y ||
        coordinator.stage.border.void_death_y != peer.stage.border.void_death_y ||
        coordinator.stage.camera_clamp_enabled != peer.stage.camera_clamp_enabled ||
        coordinator.stage.wrap_transform_active != peer.stage.wrap_transform_active ||
        coordinator.stage.wrap_padding_tiles != peer.stage.wrap_padding_tiles ||
        coordinator.stage.wrap_core_origin_tiles != peer.stage.wrap_core_origin_tiles ||
        coordinator.stage.wrap_core_size_tiles != peer.stage.wrap_core_size_tiles) {
        std::ostringstream output;
        output << "stage metadata differs c=" << coordinator.stage.quest_id << "/"
               << coordinator.stage.quest_stage_id << "/" << coordinator.stage.route_label
               << " p=" << peer.stage.quest_id << "/" << peer.stage.quest_stage_id
               << "/" << peer.stage.route_label;
        return output.str();
    }
    if (coordinator.stage.tile_change_generation != peer.stage.tile_change_generation) {
        std::ostringstream output;
        output << "tile_change_generation differs c="
               << coordinator.stage.tile_change_generation
               << " p=" << peer.stage.tile_change_generation;
        return output.str();
    }
    for (unsigned int y = 0; y < coordinator.stage.GetTileHeight(); ++y) {
        for (unsigned int x = 0; x < coordinator.stage.GetTileWidth(); ++x) {
            if (coordinator.stage.GetTile(x, y) != peer.stage.GetTile(x, y) ||
                coordinator.stage.GetTileRotation(x, y) != peer.stage.GetTileRotation(x, y) ||
                coordinator.stage.GetBackwallTile(x, y) != peer.stage.GetBackwallTile(x, y) ||
                coordinator.stage.GetFluidTile(x, y) != peer.stage.GetFluidTile(x, y) ||
                std::fabs(
                    coordinator.stage.GetFluidAmount(x, y) - peer.stage.GetFluidAmount(x, y)
                ) > 0.0001F) {
                std::ostringstream output;
                output << "stage cell differs at " << x << "," << y
                       << " tile=" << static_cast<int>(coordinator.stage.GetTile(x, y))
                       << "/" << static_cast<int>(peer.stage.GetTile(x, y))
                       << " rot=" << static_cast<int>(coordinator.stage.GetTileRotation(x, y))
                       << "/" << static_cast<int>(peer.stage.GetTileRotation(x, y))
                       << " back=" << static_cast<int>(coordinator.stage.GetBackwallTile(x, y))
                       << "/" << static_cast<int>(peer.stage.GetBackwallTile(x, y))
                       << " fluid=" << static_cast<int>(coordinator.stage.GetFluidTile(x, y))
                       << "/" << static_cast<int>(peer.stage.GetFluidTile(x, y))
                       << " amount=" << coordinator.stage.GetFluidAmount(x, y)
                       << "/" << peer.stage.GetFluidAmount(x, y);
                return output.str();
            }
        }
    }

    std::vector<const PlayerSlot*> coordinator_slots;
    std::vector<const PlayerSlot*> peer_slots;
    for (const PlayerSlot& slot : coordinator.players.slots) {
        coordinator_slots.push_back(&slot);
    }
    for (const PlayerSlot& slot : peer.players.slots) {
        peer_slots.push_back(&slot);
    }
    const auto by_player_id = [](const PlayerSlot* lhs, const PlayerSlot* rhs) {
        return lhs->player_id < rhs->player_id;
    };
    std::sort(coordinator_slots.begin(), coordinator_slots.end(), by_player_id);
    std::sort(peer_slots.begin(), peer_slots.end(), by_player_id);
    if (coordinator_slots.size() != peer_slots.size()) {
        std::ostringstream output;
        output << "player slot count differs c=" << coordinator_slots.size()
               << " p=" << peer_slots.size();
        return output.str();
    }
    for (std::size_t i = 0; i < coordinator_slots.size(); ++i) {
        const PlayerSlot& c = *coordinator_slots[i];
        const PlayerSlot& p = *peer_slots[i];
        const network::NetEntityId c_entity_id = c.entity_vid.has_value()
            ? SmokeNetIdForVid(coordinator, *c.entity_vid)
            : network::kInvalidNetEntityId;
        const network::NetEntityId p_entity_id = p.entity_vid.has_value()
            ? SmokeNetIdForVid(peer, *p.entity_vid)
            : network::kInvalidNetEntityId;
        if (c.player_id != p.player_id ||
            c.connected != p.connected ||
            c.entity_vid.has_value() != p.entity_vid.has_value() ||
            c_entity_id != p_entity_id) {
            std::ostringstream output;
            output << "player slot differs index=" << i
                   << " id=" << c.player_id << "/" << p.player_id
                   << " connected=" << c.connected << "/" << p.connected
                   << " entity=" << c_entity_id << "/" << p_entity_id;
            return output.str();
        }
    }

    std::vector<const EntityToolState*> coordinator_tools;
    std::vector<const EntityToolState*> peer_tools;
    for (const EntityToolState& tool_state : coordinator.entity_tools.tool_states) {
        if (SmokeNetIdForVid(coordinator, tool_state.owner_vid) == network::kInvalidNetEntityId) {
            continue;
        }
        coordinator_tools.push_back(&tool_state);
    }
    for (const EntityToolState& tool_state : peer.entity_tools.tool_states) {
        if (SmokeNetIdForVid(peer, tool_state.owner_vid) == network::kInvalidNetEntityId) {
            continue;
        }
        peer_tools.push_back(&tool_state);
    }
    const auto by_tool_owner = [](const State& state) {
        return [&state](const EntityToolState* lhs, const EntityToolState* rhs) {
            return SmokeNetIdForVid(state, lhs->owner_vid) <
                   SmokeNetIdForVid(state, rhs->owner_vid);
        };
    };
    std::sort(coordinator_tools.begin(), coordinator_tools.end(), by_tool_owner(coordinator));
    std::sort(peer_tools.begin(), peer_tools.end(), by_tool_owner(peer));
    if (coordinator_tools.size() != peer_tools.size()) {
        std::ostringstream output;
        output << "tool state count differs c=" << coordinator_tools.size()
               << " p=" << peer_tools.size();
        return output.str();
    }
    for (std::size_t i = 0; i < coordinator_tools.size(); ++i) {
        const EntityToolState& c = *coordinator_tools[i];
        const EntityToolState& p = *peer_tools[i];
        if (SmokeNetIdForVid(coordinator, c.owner_vid) != SmokeNetIdForVid(peer, p.owner_vid)) {
            return "tool owner net id differs";
        }
        for (std::size_t slot_index = 0; slot_index < c.slots.size(); ++slot_index) {
            const ToolSlot& c_slot = c.slots[slot_index];
            const ToolSlot& p_slot = p.slots[slot_index];
            if (c_slot.kind != p_slot.kind ||
                c_slot.count != p_slot.count ||
                c_slot.cooldown != p_slot.cooldown ||
                c_slot.active != p_slot.active) {
                std::ostringstream output;
                output << "tool slot differs owner_index=" << i
                       << " slot=" << slot_index
                       << " kind=" << static_cast<int>(c_slot.kind)
                       << "/" << static_cast<int>(p_slot.kind)
                       << " count=" << c_slot.count << "/" << p_slot.count
                       << " cooldown=" << c_slot.cooldown << "/" << p_slot.cooldown
                       << " active=" << c_slot.active << "/" << p_slot.active;
                return output.str();
            }
        }
    }

    std::vector<const Entity*> coordinator_entities;
    std::vector<const Entity*> peer_entities;
    for (const Entity& entity : coordinator.entity_manager.entities) {
        if (entity.active) {
            coordinator_entities.push_back(&entity);
        }
    }
    for (const Entity& entity : peer.entity_manager.entities) {
        if (entity.active) {
            peer_entities.push_back(&entity);
        }
    }
    const auto by_net_id = [](const State& state) {
        return [&state](const Entity* lhs, const Entity* rhs) {
            const network::NetEntityId lhs_id = SmokeNetIdForVid(state, lhs->vid);
            const network::NetEntityId rhs_id = SmokeNetIdForVid(state, rhs->vid);
            if (lhs_id != rhs_id) {
                return lhs_id < rhs_id;
            }
            return lhs->vid.id < rhs->vid.id;
        };
    };
    std::sort(coordinator_entities.begin(), coordinator_entities.end(), by_net_id(coordinator));
    std::sort(peer_entities.begin(), peer_entities.end(), by_net_id(peer));
    if (coordinator_entities.size() != peer_entities.size()) {
        std::ostringstream output;
        output << "active entity count differs c=" << coordinator_entities.size()
               << " p=" << peer_entities.size();
        return output.str();
    }
    for (std::size_t i = 0; i < coordinator_entities.size(); ++i) {
        const Entity& c = *coordinator_entities[i];
        const Entity& p = *peer_entities[i];
        const network::NetEntityId c_id = SmokeNetIdForVid(coordinator, c.vid);
        const network::NetEntityId p_id = SmokeNetIdForVid(peer, p.vid);
        if (c_id != p_id ||
            c.type_ != p.type_ ||
            c.pos != p.pos ||
            c.vel != p.vel ||
            c.acc != p.acc ||
            c.health != p.health ||
            c.condition != p.condition ||
            c.ai_state != p.ai_state ||
            c.movement_flags != p.movement_flags ||
            c.frame_data_animator.animation_id != p.frame_data_animator.animation_id ||
            c.frame_data_animator.current_frame != p.frame_data_animator.current_frame ||
            c.frame_data_animator.current_time != p.frame_data_animator.current_time ||
            c.frame_data_animator.speed != p.frame_data_animator.speed ||
            c.frame_data_animator.animate != p.frame_data_animator.animate ||
            c.frame_data_animator.loop != p.frame_data_animator.loop ||
            c.frame_data_animator.finished != p.frame_data_animator.finished) {
            std::ostringstream output;
            output << "entity differs index=" << i
                   << " net=" << c_id << "/" << p_id
                   << " type=" << static_cast<int>(c.type_) << "/" << static_cast<int>(p.type_)
                   << " pos=" << c.pos.x << "," << c.pos.y
                   << "/" << p.pos.x << "," << p.pos.y
                   << " vel=" << c.vel.x << "," << c.vel.y
                   << "/" << p.vel.x << "," << p.vel.y
                   << " acc=" << c.acc.x << "," << c.acc.y
                   << "/" << p.acc.x << "," << p.acc.y
                   << " health=" << c.health << "/" << p.health
                   << " condition=" << static_cast<int>(c.condition)
                   << "/" << static_cast<int>(p.condition)
                   << " ai=" << static_cast<int>(c.ai_state)
                   << "/" << static_cast<int>(p.ai_state)
                   << " movement=" << c.movement_flags << "/" << p.movement_flags
                   << " anim=" << c.frame_data_animator.animation_id
                   << "/" << p.frame_data_animator.animation_id
                   << " frame/time/speed=" << c.frame_data_animator.current_frame
                   << "," << c.frame_data_animator.current_time
                   << "," << c.frame_data_animator.speed
                   << "/" << p.frame_data_animator.current_frame
                   << "," << p.frame_data_animator.current_time
                   << "," << p.frame_data_animator.speed;
            return output.str();
        }
    }
    return "no packet-smoke network snapshot lane diff found";
}

bool RunLateJoinSnapshotPacketSmoke(Graphics& graphics, Audio& audio) {
    constexpr std::uint32_t seed = 67890;
    State coordinator = State::New();
    State late_peer = State::New();
    if (!LoadQuestStage(coordinator, "classic", "classic_mines_1", false, seed)) {
        std::cerr << "network packet smoke failed: could not load late join coordinator stage\n";
        return false;
    }
    ConfigureProtocolSmokeCoordinator(coordinator);
    coordinator.net_session.quest_id = coordinator.stage.quest_id;
    coordinator.net_session.quest_stage_id = coordinator.stage.quest_stage_id;
    coordinator.net_session.stage_seed = seed;

    network::NetTransportRuntime coordinator_transport = network::NetTransportRuntime::New();
    network::NetTransportRuntime late_peer_transport = network::NetTransportRuntime::New();
    coordinator_transport.capture_outgoing_packets = true;
    late_peer_transport.capture_outgoing_packets = true;
    const network::NetEndpoint coordinator_endpoint{
        .address = "127.0.0.1",
        .port = 43201,
    };
    const network::NetEndpoint late_peer_endpoint{
        .address = "127.0.0.1",
        .port = 43202,
    };
    late_peer_transport.coordinator_endpoint = coordinator_endpoint;

    const Entity* const host_player = FindFirstPlayerLikeEntity(coordinator);
    if (host_player == nullptr) {
        std::cerr << "network packet smoke failed: late join coordinator has no host player\n";
        return false;
    }
    const VID host_player_vid = host_player->vid;
    Entity* const host_player_mut = coordinator.entity_manager.GetEntityMut(host_player_vid);
    if (host_player_mut == nullptr) {
        std::cerr << "network packet smoke failed: late join could not mutate host player\n";
        return false;
    }
    host_player_mut->health = 377;
    host_player_mut->money = 1234;
    host_player_mut->pos = host_player_mut->pos + Vec2::New(12.0F, 4.0F);

    const IVec2 snapshot_tile_pos = IVec2::New(10, 10);
    const IVec2 snapshot_fluid_pos = IVec2::New(11, 10);
    coordinator.stage.SetTile(snapshot_tile_pos, Tile::CaveBlock);
    coordinator.stage.SetTileRotation(snapshot_tile_pos, kTileRotation90);
    coordinator.stage.SetBackwallTile(snapshot_tile_pos, Tile::Air);
    coordinator.stage.SetFluidTile(snapshot_fluid_pos, Tile::WaterSwim);
    Entity* const snapshot_gold = world_ops::SpawnEntity(
        coordinator,
        EntityType::GoldStack,
        [host_player_mut](Entity& entity) {
            entity.pos = host_player_mut->pos + Vec2::New(24.0F, -8.0F);
            entity.vel = Vec2::New(0.25F, -0.5F);
            entity.acc = Vec2::New(0.0F, 0.0F);
        }
    );
    if (snapshot_gold == nullptr) {
        std::cerr << "network packet smoke failed: late join could not create snapshot gold\n";
        return false;
    }
    const VID snapshot_gold_vid = snapshot_gold->vid;
    const PlayerId disconnected_player_id = 22;
    network::EnsureSpawnedPlayer(
        coordinator,
        disconnected_player_id,
        false,
        false,
        host_player_mut->pos + Vec2::New(40.0F, 0.0F),
        graphics
    );
    PlayerSlot* const disconnected_slot = coordinator.players.Find(disconnected_player_id);
    if (disconnected_slot == nullptr || !disconnected_slot->entity_vid.has_value()) {
        std::cerr << "network packet smoke failed: late join could not create disconnected player slot\n";
        return false;
    }
    Entity* const disconnected_body = coordinator.entity_manager.GetEntityMut(*disconnected_slot->entity_vid);
    if (disconnected_body == nullptr) {
        std::cerr << "network packet smoke failed: late join missing disconnected player body\n";
        return false;
    }
    disconnected_body->health = 299;
    disconnected_body->money = 7654;
    network::LeaveNoticePacket disconnected_leave;
    disconnected_leave.player_count = 1;
    disconnected_leave.player_ids[0] = disconnected_player_id;
    network::HandleLeaveNoticeAsCoordinator(
        coordinator,
        coordinator_transport,
        disconnected_leave
    );
    if (coordinator.players.Find(disconnected_player_id) != nullptr ||
        coordinator.net_session.retained_players.empty()) {
        std::cerr << "network packet smoke failed: late join did not retain/remove disconnected player\n";
        return false;
    }
    coordinator_transport.captured_packets.clear();

    coordinator.net_session.ordered_events.clear();
    network::JoinRequestPacket request;
    request.local_player_count = 1;
    network::WriteFixedString("Late", request.display_name);
    network::UdpPacket join_packet;
    join_packet.endpoint = late_peer_endpoint;
    network::HandleJoinRequestAsCoordinator(
        coordinator,
        graphics,
        coordinator_transport,
        join_packet,
        request
    );

    std::vector<network::UdpPacket> join_accept_packets =
        std::move(coordinator_transport.captured_packets);
    coordinator_transport.captured_packets.clear();
    bool accepted = false;
    std::uint64_t accepted_snapshot_start_order = 0;
    for (const network::UdpPacket& packet : join_accept_packets) {
        if (const std::optional<network::JoinAcceptPacket> accept =
                network::TryDecodeJoinAccept(packet.bytes.data(), packet.size)) {
            network::HandleJoinAcceptAsPeer(late_peer, graphics, late_peer_transport, *accept);
            if (late_peer.net_session.next_expected_coordinator_order !=
                accept->snapshot_start_coordinator_order) {
                std::cerr << "network packet smoke failed: late join accept snapshot order not applied: accept="
                          << accept->snapshot_start_coordinator_order
                          << " peer=" << late_peer.net_session.next_expected_coordinator_order
                          << '\n';
                return false;
            }
            accepted_snapshot_start_order = accept->snapshot_start_coordinator_order;
            accepted = true;
            continue;
        }
        std::cerr << "network packet smoke failed: late join coordinator emitted non-accept packet\n";
        return false;
    }
    if (!accepted) {
        std::cerr << "network packet smoke failed: late join emitted no JoinAccept\n";
        return false;
    }
    std::size_t snapshot_tile_event_count = 0;
    for (const network::NetEvent& event : coordinator.net_session.ordered_events) {
        if (event.type == network::NetEventType::TileChanged) {
            ++snapshot_tile_event_count;
        }
    }
    if (snapshot_tile_event_count == 0) {
        std::cerr << "network packet smoke failed: late join queued no tile snapshot events\n";
        return false;
    }
    std::uint64_t first_snapshot_order = 0;
    for (const network::NetEvent& event : coordinator.net_session.ordered_events) {
        if (event.header.coordinator_order != 0) {
            first_snapshot_order = event.header.coordinator_order;
            break;
        }
    }

    if (!DeliverCoordinatorPacketsToPeer(
            coordinator,
            late_peer,
            coordinator_transport,
            late_peer_transport,
            late_peer_endpoint,
            graphics,
            audio,
            "packet late join snapshot",
            false
        )) {
        return false;
    }

    const Tile late_tile = late_peer.stage.GetTile(
        static_cast<unsigned int>(snapshot_tile_pos.x),
        static_cast<unsigned int>(snapshot_tile_pos.y)
    );
    const TileRotation late_rotation = late_peer.stage.GetTileRotation(
        static_cast<unsigned int>(snapshot_tile_pos.x),
        static_cast<unsigned int>(snapshot_tile_pos.y)
    );
    const Tile late_backwall = late_peer.stage.GetBackwallTile(
        static_cast<unsigned int>(snapshot_tile_pos.x),
        static_cast<unsigned int>(snapshot_tile_pos.y)
    );
    if (late_tile != Tile::CaveBlock || late_rotation != kTileRotation90 ||
        late_backwall != Tile::Air) {
        std::cerr << "network packet smoke failed: late join tile snapshot facts differ: tile="
                  << static_cast<int>(late_tile)
                  << " rotation=" << static_cast<int>(late_rotation)
                  << " backwall=" << static_cast<int>(late_backwall)
                  << " accepted_snapshot_start_order=" << accepted_snapshot_start_order
                  << " first_snapshot_order=" << first_snapshot_order
                  << " peer_queued=" << late_peer.net_session.ordered_events.size()
                  << " next_expected=" << late_peer.net_session.next_expected_coordinator_order
                  << " highest_applied=" << late_peer.net_session.highest_applied_coordinator_order
                  << '\n';
        return false;
    }
    if (late_peer.stage.GetFluidTile(
            static_cast<unsigned int>(snapshot_fluid_pos.x),
            static_cast<unsigned int>(snapshot_fluid_pos.y)
        ) != Tile::WaterSwim ||
        late_peer.stage.GetFluidAmount(
            static_cast<unsigned int>(snapshot_fluid_pos.x),
            static_cast<unsigned int>(snapshot_fluid_pos.y)
        ) <= 0.99F) {
        std::cerr << "network packet smoke failed: late join fluid snapshot facts differ\n";
        return false;
    }
    const std::optional<VID> late_peer_gold_vid =
        FindPeerEntityForCoordinatorEntity(coordinator, late_peer, snapshot_gold_vid);
    if (!late_peer_gold_vid.has_value()) {
        const std::optional<network::NetEntityId> snapshot_gold_id =
            coordinator.net_session.FindNetEntityId(snapshot_gold_vid);
        std::uint64_t first_queued_order = 0;
        network::NetEventType first_queued_type = network::NetEventType::None;
        bool has_next_expected_order = false;
        for (const network::NetEvent& event : late_peer.net_session.ordered_events) {
            if (event.header.coordinator_order != 0) {
                if (first_queued_order == 0) {
                    first_queued_order = event.header.coordinator_order;
                    first_queued_type = event.type;
                }
                if (event.header.coordinator_order ==
                    late_peer.net_session.next_expected_coordinator_order) {
                    has_next_expected_order = true;
                }
            }
        }
        std::cerr << "network packet smoke failed: late join missing snapshot entity link"
                  << " net_id=" << snapshot_gold_id.value_or(network::kInvalidNetEntityId)
                  << " peer_links=" << late_peer.net_session.entity_links.size()
                  << " peer_queued=" << late_peer.net_session.ordered_events.size()
                  << " next_expected=" << late_peer.net_session.next_expected_coordinator_order
                  << " highest_applied=" << late_peer.net_session.highest_applied_coordinator_order
                  << " first_queued_order=" << first_queued_order
                  << " first_queued_type=" << static_cast<int>(first_queued_type)
                  << " has_next_expected=" << has_next_expected_order
                  << '\n';
        return false;
    }
    const Entity* const late_peer_gold = late_peer.entity_manager.GetEntity(*late_peer_gold_vid);
    if (late_peer_gold == nullptr || late_peer_gold->type_ != EntityType::GoldStack ||
        std::fabs(late_peer_gold->vel.x - 0.25F) > 0.001F ||
        std::fabs(late_peer_gold->vel.y + 0.5F) > 0.001F) {
        std::cerr << "network packet smoke failed: late join snapshot entity state differs\n";
        return false;
    }
    const PlayerSlot* const late_peer_host_slot =
        late_peer.players.Find(coordinator.net_session.coordinator_player_id);
    if (late_peer_host_slot == nullptr || !late_peer_host_slot->entity_vid.has_value()) {
        std::cerr << "network packet smoke failed: late join missing host player slot\n";
        return false;
    }
    const Entity* const late_peer_host_player =
        late_peer.entity_manager.GetEntity(*late_peer_host_slot->entity_vid);
    if (late_peer_host_player == nullptr || late_peer_host_player->health != 377 ||
        late_peer_host_player->money != 1234) {
        std::cerr << "network packet smoke failed: late join host player state differs\n";
        return false;
    }
    if (late_peer.players.Find(disconnected_player_id) != nullptr) {
        std::cerr << "network packet smoke failed: late join replicated retained-only player as live slot\n";
        return false;
    }
    if (!CheckSnapshotFingerprint(coordinator, late_peer, "late join")) {
        return false;
    }

    std::cout << "network packet smoke late join snapshot ok\n";
    return true;
}

bool RunReconnectPlayerIdPacketSmoke(Graphics& graphics) {
    constexpr std::uint32_t seed = 98765;
    State coordinator = State::New();
    State peer = State::New();
    if (!LoadQuestStage(coordinator, "classic", "classic_mines_1", false, seed)) {
        std::cerr << "network packet smoke failed: could not load reconnect coordinator stage\n";
        return false;
    }
    ConfigureProtocolSmokeCoordinator(coordinator);
    coordinator.net_session.quest_id = coordinator.stage.quest_id;
    coordinator.net_session.quest_stage_id = coordinator.stage.quest_stage_id;
    coordinator.net_session.stage_seed = seed;

    network::NetTransportRuntime coordinator_transport = network::NetTransportRuntime::New();
    network::NetTransportRuntime peer_transport = network::NetTransportRuntime::New();
    coordinator_transport.capture_outgoing_packets = true;
    peer_transport.capture_outgoing_packets = true;
    peer_transport.coordinator_endpoint = network::NetEndpoint{.address = "127.0.0.1", .port = 43301};
    const network::NetEndpoint first_peer_endpoint{.address = "127.0.0.1", .port = 43302};
    const network::NetEndpoint reconnect_peer_endpoint{.address = "127.0.0.1", .port = 43303};

    network::JoinRequestPacket first_request;
    first_request.local_player_count = 1;
    network::WriteFixedString("Reconnect", first_request.display_name);
    network::UdpPacket first_packet;
    first_packet.endpoint = first_peer_endpoint;
    network::HandleJoinRequestAsCoordinator(
        coordinator,
        graphics,
        coordinator_transport,
        first_packet,
        first_request
    );

    std::vector<network::UdpPacket> accept_packets = TakeCapturedPackets(coordinator_transport);
    if (accept_packets.size() != 1) {
        std::cerr << "network packet smoke failed: reconnect first join emitted "
                  << accept_packets.size() << " accept packets\n";
        return false;
    }
    const std::optional<network::JoinAcceptPacket> first_accept =
        network::TryDecodeJoinAccept(accept_packets[0].bytes.data(), accept_packets[0].size);
    if (!first_accept.has_value() || first_accept->assigned_player_count == 0) {
        std::cerr << "network packet smoke failed: reconnect first accept missing\n";
        return false;
    }
    network::HandleJoinAcceptAsPeer(peer, graphics, peer_transport, *first_accept);
    const PlayerId assigned_player_id = first_accept->assigned_player_ids[0];
    if (assigned_player_id == kInvalidPlayerId ||
        peer_transport.preferred_player_ids.empty() ||
        peer_transport.preferred_player_ids[0] != assigned_player_id) {
        std::cerr << "network packet smoke failed: reconnect peer did not retain assigned player id\n";
        return false;
    }

    PlayerSlot* const joined_slot = coordinator.players.Find(assigned_player_id);
    if (joined_slot == nullptr || !joined_slot->entity_vid.has_value() || !joined_slot->connected) {
        std::cerr << "network packet smoke failed: reconnect coordinator missing joined slot\n";
        return false;
    }
    Entity* const joined_entity = coordinator.entity_manager.GetEntityMut(*joined_slot->entity_vid);
    if (joined_entity == nullptr) {
        std::cerr << "network packet smoke failed: reconnect coordinator missing joined entity\n";
        return false;
    }
    const VID joined_vid = joined_entity->vid;
    joined_entity->health = 321;
    joined_entity->money = 6543;
    joined_entity->pos = joined_entity->pos + Vec2::New(31.0F, -7.0F);
    const Vec2 resumed_pos = joined_entity->pos;
    Entity* const held_rock = world_ops::SpawnEntity(
        coordinator,
        EntityType::Rock,
        [&](Entity& entity) {
            entity.pos = joined_entity->pos + Vec2::New(4.0F, 0.0F);
            entity.vel = Vec2::New(1.0F, -2.0F);
            entity.health = 77;
        }
    );
    Entity* const back_cape = world_ops::SpawnEntity(
        coordinator,
        EntityType::Cape,
        [&](Entity& entity) {
            entity.pos = joined_entity->pos;
            entity.health = 88;
        }
    );
    if (held_rock == nullptr || back_cape == nullptr) {
        std::cerr << "network packet smoke failed: reconnect setup missing retained items\n";
        return false;
    }
    const VID held_rock_vid = held_rock->vid;
    const VID back_cape_vid = back_cape->vid;
    joined_entity->holding_vid = held_rock->vid;
    joined_entity->holding = true;
    held_rock->held_by_vid = joined_entity->vid;
    held_rock->attachment_mode = AttachmentMode::Held;
    held_rock->has_physics = false;
    held_rock->can_collide = false;
    joined_entity->back_vid = back_cape->vid;
    back_cape->held_by_vid = joined_entity->vid;
    back_cape->attachment_mode = AttachmentMode::Back;
    back_cape->has_physics = false;
    back_cape->can_collide = false;

    network::LeaveNoticePacket leave;
    leave.player_count = 1;
    leave.player_ids[0] = assigned_player_id;
    network::HandleLeaveNoticeAsCoordinator(coordinator, coordinator_transport, leave);
    const PlayerSlot* const disconnected_slot = coordinator.players.Find(assigned_player_id);
    if (disconnected_slot != nullptr) {
        std::cerr << "network packet smoke failed: reconnect leave kept disconnected player live slot\n";
        return false;
    }
    if (joined_vid.id >= coordinator.entity_manager.entities.size() ||
        coordinator.entity_manager.entities[joined_vid.id].active) {
        std::cerr << "network packet smoke failed: reconnect leave did not deactivate live body\n";
        return false;
    }
    if (held_rock_vid.id >= coordinator.entity_manager.entities.size() ||
        coordinator.entity_manager.entities[held_rock_vid.id].active ||
        back_cape_vid.id >= coordinator.entity_manager.entities.size() ||
        coordinator.entity_manager.entities[back_cape_vid.id].active) {
        std::cerr << "network packet smoke failed: reconnect leave did not deactivate retained items\n";
        return false;
    }
    const auto retained_it = std::find_if(
        coordinator.net_session.retained_players.begin(),
        coordinator.net_session.retained_players.end(),
        [assigned_player_id](const network::NetRetainedPlayerState& retained) {
            return retained.player_id == assigned_player_id;
        }
    );
    if (retained_it == coordinator.net_session.retained_players.end() ||
        retained_it->health != 321 ||
        retained_it->money != 6543 ||
        retained_it->last_pos != resumed_pos ||
        !retained_it->held_item.valid ||
        retained_it->held_item.entity_type != EntityType::Rock ||
        !retained_it->back_item.valid ||
        retained_it->back_item.entity_type != EntityType::Cape) {
        std::cerr << "network packet smoke failed: reconnect leave did not retain player body state\n";
        return false;
    }

    coordinator.net_session.ordered_events.clear();
    network::JoinRequestPacket reconnect_request;
    reconnect_request.local_player_count = 1;
    reconnect_request.preferred_player_count = 1;
    reconnect_request.preferred_player_ids[0] = assigned_player_id;
    network::WriteFixedString("Reconnect", reconnect_request.display_name);
    network::UdpPacket reconnect_packet;
    reconnect_packet.endpoint = reconnect_peer_endpoint;
    network::HandleJoinRequestAsCoordinator(
        coordinator,
        graphics,
        coordinator_transport,
        reconnect_packet,
        reconnect_request
    );

    accept_packets = TakeCapturedPackets(coordinator_transport);
    if (accept_packets.size() != 1) {
        std::cerr << "network packet smoke failed: reconnect second join emitted "
                  << accept_packets.size() << " accept packets\n";
        return false;
    }
    const std::optional<network::JoinAcceptPacket> reconnect_accept =
        network::TryDecodeJoinAccept(accept_packets[0].bytes.data(), accept_packets[0].size);
    if (!reconnect_accept.has_value() ||
        reconnect_accept->assigned_player_count == 0 ||
        reconnect_accept->assigned_player_ids[0] != assigned_player_id) {
        std::cerr << "network packet smoke failed: reconnect did not reuse preferred player id\n";
        return false;
    }
    const PlayerSlot* const resumed_slot = coordinator.players.Find(assigned_player_id);
    const Entity* const resumed_entity = resumed_slot != nullptr && resumed_slot->entity_vid.has_value()
        ? coordinator.entity_manager.GetEntity(*resumed_slot->entity_vid)
        : nullptr;
    if (resumed_slot == nullptr ||
        !resumed_slot->connected ||
        resumed_entity == nullptr ||
        resumed_entity->vid == joined_vid ||
        resumed_entity->health != 321 ||
        resumed_entity->money != 6543 ||
        resumed_entity->pos != resumed_pos) {
        std::cerr << "network packet smoke failed: reconnect did not restore retained body state\n";
        return false;
    }
    if (!resumed_entity->holding_vid.has_value() || !resumed_entity->back_vid.has_value()) {
        std::cerr << "network packet smoke failed: reconnect did not restore retained attachments\n";
        return false;
    }
    const Entity* const resumed_held =
        coordinator.entity_manager.GetEntity(*resumed_entity->holding_vid);
    const Entity* const resumed_back =
        coordinator.entity_manager.GetEntity(*resumed_entity->back_vid);
    if (resumed_held == nullptr ||
        !resumed_held->active ||
        resumed_held->type_ != EntityType::Rock ||
        resumed_held->held_by_vid != resumed_entity->vid ||
        resumed_held->attachment_mode != AttachmentMode::Held ||
        resumed_back == nullptr ||
        !resumed_back->active ||
        resumed_back->type_ != EntityType::Cape ||
        resumed_back->held_by_vid != resumed_entity->vid ||
        resumed_back->attachment_mode != AttachmentMode::Back) {
        std::cerr << "network packet smoke failed: reconnect restored attachments incorrectly\n";
        return false;
    }
    if (std::any_of(
            coordinator.net_session.retained_players.begin(),
            coordinator.net_session.retained_players.end(),
            [assigned_player_id](const network::NetRetainedPlayerState& retained) {
                return retained.player_id == assigned_player_id;
            }
        )) {
        std::cerr << "network packet smoke failed: reconnect did not consume retained player state\n";
        return false;
    }

    std::cout << "network packet smoke reconnect player id ok\n";
    return true;
}

bool RunReconnectAfterWorldMutationPacketSmoke(Graphics& graphics, Audio& audio) {
    constexpr std::uint32_t seed = 98769;
    State coordinator = State::New();
    State reconnect_peer = State::New();
    if (!LoadQuestStage(coordinator, "classic", "classic_mines_1", false, seed)) {
        std::cerr << "network packet smoke failed: could not load reconnect-mutation coordinator stage\n";
        return false;
    }
    ConfigureProtocolSmokeCoordinator(coordinator);
    coordinator.net_session.quest_id = coordinator.stage.quest_id;
    coordinator.net_session.quest_stage_id = coordinator.stage.quest_stage_id;
    coordinator.net_session.stage_seed = seed;

    network::NetTransportRuntime coordinator_transport = network::NetTransportRuntime::New();
    network::NetTransportRuntime reconnect_peer_transport = network::NetTransportRuntime::New();
    coordinator_transport.capture_outgoing_packets = true;
    reconnect_peer_transport.capture_outgoing_packets = true;
    reconnect_peer_transport.coordinator_endpoint = network::NetEndpoint{
        .address = "127.0.0.1",
        .port = 43341,
    };
    const network::NetEndpoint first_peer_endpoint{.address = "127.0.0.1", .port = 43342};
    const network::NetEndpoint reconnect_peer_endpoint{.address = "127.0.0.1", .port = 43343};

    network::JoinRequestPacket first_request;
    first_request.local_player_count = 1;
    network::WriteFixedString("WorldRejoin", first_request.display_name);
    network::UdpPacket first_packet;
    first_packet.endpoint = first_peer_endpoint;
    network::HandleJoinRequestAsCoordinator(
        coordinator,
        graphics,
        coordinator_transport,
        first_packet,
        first_request
    );
    std::vector<network::UdpPacket> accept_packets = TakeCapturedPackets(coordinator_transport);
    if (accept_packets.size() != 1) {
        std::cerr << "network packet smoke failed: reconnect-mutation first join emitted "
                  << accept_packets.size() << " accept packets\n";
        return false;
    }
    const std::optional<network::JoinAcceptPacket> first_accept =
        network::TryDecodeJoinAccept(accept_packets[0].bytes.data(), accept_packets[0].size);
    if (!first_accept.has_value() || first_accept->assigned_player_count == 0) {
        std::cerr << "network packet smoke failed: reconnect-mutation first accept missing\n";
        return false;
    }
    const PlayerId assigned_player_id = first_accept->assigned_player_ids[0];
    PlayerSlot* const joined_slot = coordinator.players.Find(assigned_player_id);
    if (joined_slot == nullptr || !joined_slot->entity_vid.has_value()) {
        std::cerr << "network packet smoke failed: reconnect-mutation missing joined player body\n";
        return false;
    }
    Entity* const joined_body = coordinator.entity_manager.GetEntityMut(*joined_slot->entity_vid);
    if (joined_body == nullptr) {
        std::cerr << "network packet smoke failed: reconnect-mutation joined body missing\n";
        return false;
    }
    joined_body->health = 234;
    joined_body->money = 8765;
    joined_body->pos = joined_body->pos + Vec2::New(19.0F, -5.0F);

    network::LeaveNoticePacket leave;
    leave.player_count = 1;
    leave.player_ids[0] = assigned_player_id;
    network::HandleLeaveNoticeAsCoordinator(coordinator, coordinator_transport, leave);

    const IVec2 changed_tile_pos = IVec2::New(12, 8);
    const IVec2 changed_fluid_pos = IVec2::New(13, 8);
    coordinator.stage.SetTile(changed_tile_pos, Tile::CaveBlock);
    coordinator.stage.SetTileRotation(changed_tile_pos, kTileRotation180);
    coordinator.stage.SetBackwallTile(changed_tile_pos, Tile::Air);
    coordinator.stage.SetFluidTile(changed_fluid_pos, Tile::WaterSwim);
    Entity* const spawned_gem = world_ops::SpawnEntity(
        coordinator,
        EntityType::EmeraldBig,
        [](Entity& entity) {
            entity.pos = Vec2::New(144.0F, 96.0F);
            entity.vel = Vec2::New(0.75F, -0.25F);
            entity.acc = Vec2::New(0.0F, 0.0F);
        }
    );
    if (spawned_gem == nullptr) {
        std::cerr << "network packet smoke failed: reconnect-mutation could not spawn gem\n";
        return false;
    }
    const VID coordinator_gem_vid = spawned_gem->vid;
    if (coordinator.players.Find(assigned_player_id) != nullptr) {
        std::cerr << "network packet smoke failed: reconnect-mutation retained player still had live slot\n";
        return false;
    }

    coordinator.net_session.ordered_events.clear();
    network::JoinRequestPacket reconnect_request;
    reconnect_request.local_player_count = 1;
    reconnect_request.preferred_player_count = 1;
    reconnect_request.preferred_player_ids[0] = assigned_player_id;
    network::WriteFixedString("WorldRejoin", reconnect_request.display_name);
    network::UdpPacket reconnect_packet;
    reconnect_packet.endpoint = reconnect_peer_endpoint;
    network::HandleJoinRequestAsCoordinator(
        coordinator,
        graphics,
        coordinator_transport,
        reconnect_packet,
        reconnect_request
    );
    accept_packets = TakeCapturedPackets(coordinator_transport);
    if (accept_packets.size() != 1) {
        std::cerr << "network packet smoke failed: reconnect-mutation second join emitted "
                  << accept_packets.size() << " accept packets\n";
        return false;
    }
    const std::optional<network::JoinAcceptPacket> reconnect_accept =
        network::TryDecodeJoinAccept(accept_packets[0].bytes.data(), accept_packets[0].size);
    if (!reconnect_accept.has_value() ||
        reconnect_accept->assigned_player_count == 0 ||
        reconnect_accept->assigned_player_ids[0] != assigned_player_id) {
        std::cerr << "network packet smoke failed: reconnect-mutation did not reuse preferred player id\n";
        return false;
    }
    network::HandleJoinAcceptAsPeer(
        reconnect_peer,
        graphics,
        reconnect_peer_transport,
        *reconnect_accept
    );
    if (!DeliverCoordinatorPacketsToPeer(
            coordinator,
            reconnect_peer,
            coordinator_transport,
            reconnect_peer_transport,
            reconnect_peer_endpoint,
            graphics,
            audio,
            "packet reconnect world mutation snapshot",
            false
        )) {
        return false;
    }

    if (reconnect_peer.stage.GetTile(
            static_cast<unsigned int>(changed_tile_pos.x),
            static_cast<unsigned int>(changed_tile_pos.y)
        ) != Tile::CaveBlock ||
        reconnect_peer.stage.GetTileRotation(
            static_cast<unsigned int>(changed_tile_pos.x),
            static_cast<unsigned int>(changed_tile_pos.y)
        ) != kTileRotation180 ||
        reconnect_peer.stage.GetBackwallTile(
            static_cast<unsigned int>(changed_tile_pos.x),
            static_cast<unsigned int>(changed_tile_pos.y)
        ) != Tile::Air) {
        std::cerr << "network packet smoke failed: reconnect-mutation tile snapshot differs\n";
        return false;
    }
    if (reconnect_peer.stage.GetFluidTile(
            static_cast<unsigned int>(changed_fluid_pos.x),
            static_cast<unsigned int>(changed_fluid_pos.y)
        ) != Tile::WaterSwim ||
        reconnect_peer.stage.GetFluidAmount(
            static_cast<unsigned int>(changed_fluid_pos.x),
            static_cast<unsigned int>(changed_fluid_pos.y)
        ) <= 0.99F) {
        std::cerr << "network packet smoke failed: reconnect-mutation fluid snapshot differs\n";
        return false;
    }
    const std::optional<VID> peer_gem_vid =
        FindPeerEntityForCoordinatorEntity(coordinator, reconnect_peer, coordinator_gem_vid);
    const Entity* const peer_gem = peer_gem_vid.has_value()
        ? reconnect_peer.entity_manager.GetEntity(*peer_gem_vid)
        : nullptr;
    if (peer_gem == nullptr ||
        peer_gem->type_ != EntityType::EmeraldBig ||
        std::fabs(peer_gem->vel.x - 0.75F) > 0.001F ||
        std::fabs(peer_gem->vel.y + 0.25F) > 0.001F) {
        std::cerr << "network packet smoke failed: reconnect-mutation spawned entity snapshot differs\n";
        return false;
    }
    const PlayerSlot* const peer_slot = reconnect_peer.players.Find(assigned_player_id);
    const Entity* const peer_body = peer_slot != nullptr && peer_slot->entity_vid.has_value()
        ? reconnect_peer.entity_manager.GetEntity(*peer_slot->entity_vid)
        : nullptr;
    if (peer_slot == nullptr ||
        !peer_slot->connected ||
        peer_body == nullptr ||
        peer_body->health != 234 ||
        peer_body->money != 8765) {
        std::cerr << "network packet smoke failed: reconnect-mutation player body snapshot differs\n";
        return false;
    }
    if (!CheckSnapshotFingerprint(coordinator, reconnect_peer, "reconnect-mutation")) {
        return false;
    }

    std::cout << "network packet smoke reconnect after world mutation ok\n";
    return true;
}

bool RunFreshReconnectPlayerIdPacketSmoke(Graphics& graphics) {
    constexpr std::uint32_t seed = 98766;
    State coordinator = State::New();
    if (!LoadQuestStage(coordinator, "classic", "classic_mines_1", false, seed)) {
        std::cerr << "network packet smoke failed: could not load fresh reconnect coordinator stage\n";
        return false;
    }
    ConfigureProtocolSmokeCoordinator(coordinator);
    coordinator.net_session.quest_id = coordinator.stage.quest_id;
    coordinator.net_session.quest_stage_id = coordinator.stage.quest_stage_id;
    coordinator.net_session.stage_seed = seed;

    network::NetTransportRuntime coordinator_transport = network::NetTransportRuntime::New();
    coordinator_transport.capture_outgoing_packets = true;
    const network::NetEndpoint first_peer_endpoint{.address = "127.0.0.1", .port = 43312};
    const network::NetEndpoint reconnect_peer_endpoint{.address = "127.0.0.1", .port = 43313};

    network::JoinRequestPacket first_request;
    first_request.local_player_count = 1;
    network::WriteFixedString("Fresh", first_request.display_name);
    network::UdpPacket first_packet;
    first_packet.endpoint = first_peer_endpoint;
    network::HandleJoinRequestAsCoordinator(
        coordinator,
        graphics,
        coordinator_transport,
        first_packet,
        first_request
    );

    std::vector<network::UdpPacket> accept_packets = TakeCapturedPackets(coordinator_transport);
    if (accept_packets.size() != 1) {
        std::cerr << "network packet smoke failed: fresh reconnect first join emitted "
                  << accept_packets.size() << " accept packets\n";
        return false;
    }
    const std::optional<network::JoinAcceptPacket> first_accept =
        network::TryDecodeJoinAccept(accept_packets[0].bytes.data(), accept_packets[0].size);
    if (!first_accept.has_value() || first_accept->assigned_player_count == 0) {
        std::cerr << "network packet smoke failed: fresh reconnect first accept missing\n";
        return false;
    }
    const PlayerId old_player_id = first_accept->assigned_player_ids[0];
    const PlayerSlot* const old_joined_slot = coordinator.players.Find(old_player_id);
    const VID old_vid = old_joined_slot != nullptr && old_joined_slot->entity_vid.has_value()
        ? *old_joined_slot->entity_vid
        : VID{};
    if (old_player_id == kInvalidPlayerId || old_vid == VID{}) {
        std::cerr << "network packet smoke failed: fresh reconnect missing first body\n";
        return false;
    }

    network::LeaveNoticePacket leave;
    leave.player_count = 1;
    leave.player_ids[0] = old_player_id;
    network::HandleLeaveNoticeAsCoordinator(coordinator, coordinator_transport, leave);
    const PlayerSlot* const disconnected_slot = coordinator.players.Find(old_player_id);
    if (disconnected_slot != nullptr ||
        std::none_of(
            coordinator.net_session.retained_players.begin(),
            coordinator.net_session.retained_players.end(),
            [old_player_id](const network::NetRetainedPlayerState& retained) {
                return retained.player_id == old_player_id;
            }
        )) {
        std::cerr << "network packet smoke failed: fresh reconnect old player was not retained off-world\n";
        return false;
    }

    coordinator.net_session.ordered_events.clear();
    network::JoinRequestPacket fresh_request;
    fresh_request.local_player_count = 1;
    network::WriteFixedString("Fresh", fresh_request.display_name);
    network::UdpPacket reconnect_packet;
    reconnect_packet.endpoint = reconnect_peer_endpoint;
    network::HandleJoinRequestAsCoordinator(
        coordinator,
        graphics,
        coordinator_transport,
        reconnect_packet,
        fresh_request
    );

    accept_packets = TakeCapturedPackets(coordinator_transport);
    if (accept_packets.size() != 1) {
        std::cerr << "network packet smoke failed: fresh reconnect second join emitted "
                  << accept_packets.size() << " accept packets\n";
        return false;
    }
    const std::optional<network::JoinAcceptPacket> fresh_accept =
        network::TryDecodeJoinAccept(accept_packets[0].bytes.data(), accept_packets[0].size);
    if (!fresh_accept.has_value() ||
        fresh_accept->assigned_player_count == 0 ||
        fresh_accept->assigned_player_ids[0] == old_player_id) {
        std::cerr << "network packet smoke failed: fresh reconnect did not allocate a new player id\n";
        return false;
    }
    const PlayerId new_player_id = fresh_accept->assigned_player_ids[0];
    const PlayerSlot* const old_slot_after_fresh = coordinator.players.Find(old_player_id);
    const PlayerSlot* const new_slot = coordinator.players.Find(new_player_id);
    const bool old_body_active = old_vid.id < coordinator.entity_manager.entities.size() &&
        coordinator.entity_manager.entities[old_vid.id].active;
    if (old_slot_after_fresh != nullptr ||
        old_body_active ||
        new_slot == nullptr ||
        !new_slot->connected ||
        !new_slot->entity_vid.has_value() ||
        *new_slot->entity_vid == old_vid) {
        std::cerr << "network packet smoke failed: fresh reconnect did not keep old body off-world and spawn new body\n";
        return false;
    }

    std::cout << "network packet smoke fresh reconnect player id ok\n";
    return true;
}

const char* ReconnectModeName(network::NetReconnectSpawnMode mode) {
    switch (mode) {
    case network::NetReconnectSpawnMode::FreshAtEntrance:
        return "fresh at entrance";
    case network::NetReconnectSpawnMode::FreshAtHost:
        return "fresh at host";
    case network::NetReconnectSpawnMode::RetainedAtEntrance:
        return "retained at entrance";
    case network::NetReconnectSpawnMode::RetainedAtLastPosition:
        return "retained at last position";
    case network::NetReconnectSpawnMode::RetainedAtHost:
        return "retained at host";
    }
    return "unknown";
}

std::optional<Vec2> FindSmokeEntranceSpawnPos(const State& state) {
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

bool VecNear(Vec2 lhs, Vec2 rhs, float epsilon = 0.001F) {
    return std::fabs(lhs.x - rhs.x) <= epsilon && std::fabs(lhs.y - rhs.y) <= epsilon;
}

bool RunReconnectSpawnModesPacketSmoke(Graphics& graphics) {
    constexpr std::uint32_t seed = 98765;
    constexpr PlayerId kHostPlayerId = 1;
    constexpr std::uint32_t kRetainedHealth = 222;
    constexpr std::uint32_t kRetainedMoney = 3333;
    const Vec2 kHostPos = Vec2::New(96.0F, 80.0F);
    const Vec2 kRetainedPos = Vec2::New(144.0F, 64.0F);
    const network::NetReconnectSpawnMode modes[] = {
        network::NetReconnectSpawnMode::FreshAtEntrance,
        network::NetReconnectSpawnMode::FreshAtHost,
        network::NetReconnectSpawnMode::RetainedAtEntrance,
        network::NetReconnectSpawnMode::RetainedAtLastPosition,
        network::NetReconnectSpawnMode::RetainedAtHost,
    };

    for (const network::NetReconnectSpawnMode mode : modes) {
        State coordinator = State::New();
        if (!LoadQuestStage(coordinator, "classic", "classic_mines_1", false, seed)) {
            std::cerr << "network packet smoke failed: could not load reconnect mode stage\n";
            return false;
        }
        ConfigureProtocolSmokeCoordinator(coordinator);
        coordinator.net_session.quest_id = coordinator.stage.quest_id;
        coordinator.net_session.quest_stage_id = coordinator.stage.quest_stage_id;
        coordinator.net_session.stage_seed = seed;
        coordinator.net_session.reconnect_spawn_mode = mode;
        network::EnsureSpawnedPlayer(coordinator, kHostPlayerId, true, true, kHostPos, graphics);

        const std::optional<Vec2> entrance_pos = FindSmokeEntranceSpawnPos(coordinator);
        if (!entrance_pos.has_value()) {
            std::cerr << "network packet smoke failed: reconnect mode test could not find entrance\n";
            return false;
        }

        network::NetTransportRuntime coordinator_transport = network::NetTransportRuntime::New();
        coordinator_transport.capture_outgoing_packets = true;
        const network::NetEndpoint first_endpoint{.address = "127.0.0.1", .port = 43400};
        const network::NetEndpoint reconnect_endpoint{.address = "127.0.0.1", .port = 43401};

        network::JoinRequestPacket first_request;
        first_request.local_player_count = 1;
        network::WriteFixedString("Mode", first_request.display_name);
        network::UdpPacket first_packet;
        first_packet.endpoint = first_endpoint;
        network::HandleJoinRequestAsCoordinator(
            coordinator,
            graphics,
            coordinator_transport,
            first_packet,
            first_request
        );
        std::vector<network::UdpPacket> accept_packets = TakeCapturedPackets(coordinator_transport);
        if (accept_packets.size() != 1) {
            std::cerr << "network packet smoke failed: reconnect mode first join emitted "
                      << accept_packets.size() << " accept packets\n";
            return false;
        }
        const std::optional<network::JoinAcceptPacket> first_accept =
            network::TryDecodeJoinAccept(accept_packets[0].bytes.data(), accept_packets[0].size);
        if (!first_accept.has_value() || first_accept->assigned_player_count == 0) {
            std::cerr << "network packet smoke failed: reconnect mode first accept missing\n";
            return false;
        }
        const PlayerId player_id = first_accept->assigned_player_ids[0];
        PlayerSlot* const first_slot = coordinator.players.Find(player_id);
        Entity* const first_body = first_slot != nullptr && first_slot->entity_vid.has_value()
            ? coordinator.entity_manager.GetEntityMut(*first_slot->entity_vid)
            : nullptr;
        if (first_body == nullptr) {
            std::cerr << "network packet smoke failed: reconnect mode missing first body\n";
            return false;
        }
        const VID first_vid = first_body->vid;
        first_body->pos = kRetainedPos;
        first_body->health = kRetainedHealth;
        first_body->money = kRetainedMoney;

        network::LeaveNoticePacket leave;
        leave.player_count = 1;
        leave.player_ids[0] = player_id;
        network::HandleLeaveNoticeAsCoordinator(coordinator, coordinator_transport, leave);
        if (coordinator.players.Find(player_id) != nullptr ||
            first_vid.id >= coordinator.entity_manager.entities.size() ||
            coordinator.entity_manager.entities[first_vid.id].active) {
            std::cerr << "network packet smoke failed: reconnect mode did not remove disconnected body for "
                      << ReconnectModeName(mode) << '\n';
            return false;
        }

        coordinator.net_session.ordered_events.clear();
        network::JoinRequestPacket reconnect_request;
        reconnect_request.local_player_count = 1;
        reconnect_request.preferred_player_count = 1;
        reconnect_request.preferred_player_ids[0] = player_id;
        network::WriteFixedString("Mode", reconnect_request.display_name);
        network::UdpPacket reconnect_packet;
        reconnect_packet.endpoint = reconnect_endpoint;
        network::HandleJoinRequestAsCoordinator(
            coordinator,
            graphics,
            coordinator_transport,
            reconnect_packet,
            reconnect_request
        );
        accept_packets = TakeCapturedPackets(coordinator_transport);
        if (accept_packets.size() != 1) {
            std::cerr << "network packet smoke failed: reconnect mode second join emitted "
                      << accept_packets.size() << " accept packets\n";
            return false;
        }
        const std::optional<network::JoinAcceptPacket> reconnect_accept =
            network::TryDecodeJoinAccept(accept_packets[0].bytes.data(), accept_packets[0].size);
        if (!reconnect_accept.has_value() ||
            reconnect_accept->assigned_player_count == 0 ||
            reconnect_accept->assigned_player_ids[0] != player_id) {
            std::cerr << "network packet smoke failed: reconnect mode did not reuse preferred player id for "
                      << ReconnectModeName(mode) << '\n';
            return false;
        }

        const PlayerSlot* const resumed_slot = coordinator.players.Find(player_id);
        const Entity* const resumed_body =
            resumed_slot != nullptr && resumed_slot->entity_vid.has_value()
                ? coordinator.entity_manager.GetEntity(*resumed_slot->entity_vid)
                : nullptr;
        if (resumed_body == nullptr || resumed_body->vid == first_vid) {
            std::cerr << "network packet smoke failed: reconnect mode missing new resumed body for "
                      << ReconnectModeName(mode) << '\n';
            return false;
        }

        Vec2 expected_pos = *entrance_pos;
        if (mode == network::NetReconnectSpawnMode::FreshAtHost ||
            mode == network::NetReconnectSpawnMode::RetainedAtHost) {
            expected_pos = kHostPos + Vec2::New(16.0F, 0.0F);
        } else if (mode == network::NetReconnectSpawnMode::RetainedAtLastPosition) {
            expected_pos = kRetainedPos;
        }
        if (!VecNear(resumed_body->pos, expected_pos)) {
            std::cerr << "network packet smoke failed: reconnect mode spawned at wrong position for "
                      << ReconnectModeName(mode) << " expected=" << expected_pos.x << ","
                      << expected_pos.y << " actual=" << resumed_body->pos.x << ","
                      << resumed_body->pos.y << '\n';
            return false;
        }

        const bool retained_mode =
            mode == network::NetReconnectSpawnMode::RetainedAtEntrance ||
            mode == network::NetReconnectSpawnMode::RetainedAtLastPosition ||
            mode == network::NetReconnectSpawnMode::RetainedAtHost;
        if (retained_mode) {
            if (resumed_body->health != kRetainedHealth || resumed_body->money != kRetainedMoney) {
                std::cerr << "network packet smoke failed: reconnect mode did not retain body values for "
                          << ReconnectModeName(mode) << '\n';
                return false;
            }
        } else if (resumed_body->health == kRetainedHealth || resumed_body->money == kRetainedMoney) {
            std::cerr << "network packet smoke failed: fresh reconnect mode kept retained values for "
                      << ReconnectModeName(mode) << '\n';
            return false;
        }
    }

    std::cout << "network packet smoke reconnect spawn modes ok\n";
    return true;
}

bool RunMultiLocalJoinPacketSmoke(Graphics& graphics) {
    constexpr std::uint32_t seed = 98767;
    State coordinator = State::New();
    State peer = State::New();
    if (!LoadQuestStage(coordinator, "classic", "classic_mines_1", false, seed)) {
        std::cerr << "network packet smoke failed: could not load multi-local coordinator stage\n";
        return false;
    }
    ConfigureProtocolSmokeCoordinator(coordinator);
    coordinator.net_session.quest_id = coordinator.stage.quest_id;
    coordinator.net_session.quest_stage_id = coordinator.stage.quest_stage_id;
    coordinator.net_session.stage_seed = seed;

    network::NetTransportRuntime coordinator_transport = network::NetTransportRuntime::New();
    network::NetTransportRuntime peer_transport = network::NetTransportRuntime::New();
    coordinator_transport.capture_outgoing_packets = true;
    peer_transport.capture_outgoing_packets = true;
    peer_transport.coordinator_endpoint = network::NetEndpoint{.address = "127.0.0.1", .port = 43321};
    const network::NetEndpoint peer_endpoint{.address = "127.0.0.1", .port = 43322};

    network::JoinRequestPacket request;
    request.local_player_count = 3;
    network::WriteFixedString("Couch", request.display_name);
    network::UdpPacket packet;
    packet.endpoint = peer_endpoint;
    network::HandleJoinRequestAsCoordinator(
        coordinator,
        graphics,
        coordinator_transport,
        packet,
        request
    );

    std::vector<network::UdpPacket> accept_packets = TakeCapturedPackets(coordinator_transport);
    if (accept_packets.size() != 1) {
        std::cerr << "network packet smoke failed: multi-local join emitted "
                  << accept_packets.size() << " accept packets\n";
        return false;
    }
    const std::optional<network::JoinAcceptPacket> accept =
        network::TryDecodeJoinAccept(accept_packets[0].bytes.data(), accept_packets[0].size);
    if (!accept.has_value() || accept->assigned_player_count != 3) {
        std::cerr << "network packet smoke failed: multi-local accept did not assign 3 players\n";
        return false;
    }
    network::HandleJoinAcceptAsPeer(peer, graphics, peer_transport, *accept);

    std::size_t coordinator_remote_count = 0;
    std::size_t peer_local_count = 0;
    for (std::uint32_t i = 0; i < accept->assigned_player_count; ++i) {
        const PlayerId player_id = accept->assigned_player_ids[i];
        const PlayerSlot* const coordinator_slot = coordinator.players.Find(player_id);
        const PlayerSlot* const peer_slot = peer.players.Find(player_id);
        if (player_id == kInvalidPlayerId ||
            coordinator_slot == nullptr ||
            peer_slot == nullptr ||
            !coordinator_slot->connected ||
            !peer_slot->connected ||
            !coordinator_slot->entity_vid.has_value() ||
            !peer_slot->entity_vid.has_value()) {
            std::cerr << "network packet smoke failed: multi-local missing assigned player slot\n";
            return false;
        }
        if (coordinator_slot->connection_kind == PlayerConnectionKind::Remote) {
            ++coordinator_remote_count;
        }
        if (peer_slot->connection_kind == PlayerConnectionKind::Local) {
            ++peer_local_count;
        }
    }
    if (coordinator_remote_count != 3 || peer_local_count != 3 ||
        coordinator_transport.remotes.size() != 1 ||
        coordinator_transport.remotes[0].player_ids.size() != 3) {
        std::cerr << "network packet smoke failed: multi-local endpoint/player ownership mismatch\n";
        return false;
    }

    std::cout << "network packet smoke multi-local join ok\n";
    return true;
}

bool RunMultiPeerJoinPacketSmoke(Graphics& graphics) {
    constexpr std::uint32_t seed = 98768;
    State coordinator = State::New();
    if (!LoadQuestStage(coordinator, "classic", "classic_mines_1", false, seed)) {
        std::cerr << "network packet smoke failed: could not load multi-peer coordinator stage\n";
        return false;
    }
    ConfigureProtocolSmokeCoordinator(coordinator);
    coordinator.net_session.quest_id = coordinator.stage.quest_id;
    coordinator.net_session.quest_stage_id = coordinator.stage.quest_stage_id;
    coordinator.net_session.stage_seed = seed;

    network::NetTransportRuntime coordinator_transport = network::NetTransportRuntime::New();
    coordinator_transport.capture_outgoing_packets = true;
    const network::NetEndpoint first_endpoint{.address = "127.0.0.1", .port = 43331};
    const network::NetEndpoint second_endpoint{.address = "127.0.0.1", .port = 43332};

    const auto join_one_peer = [&](const network::NetEndpoint& endpoint, const char* name) -> PlayerId {
        network::JoinRequestPacket request;
        request.local_player_count = 1;
        network::WriteFixedString(name, request.display_name);
        network::UdpPacket packet;
        packet.endpoint = endpoint;
        network::HandleJoinRequestAsCoordinator(
            coordinator,
            graphics,
            coordinator_transport,
            packet,
            request
        );
        std::vector<network::UdpPacket> accept_packets = TakeCapturedPackets(coordinator_transport);
        if (accept_packets.size() != 1) {
            return kInvalidPlayerId;
        }
        const std::optional<network::JoinAcceptPacket> accept =
            network::TryDecodeJoinAccept(accept_packets[0].bytes.data(), accept_packets[0].size);
        if (!accept.has_value() || accept->assigned_player_count != 1) {
            return kInvalidPlayerId;
        }
        return accept->assigned_player_ids[0];
    };

    const PlayerId first_player_id = join_one_peer(first_endpoint, "PeerA");
    const PlayerId second_player_id = join_one_peer(second_endpoint, "PeerB");
    if (first_player_id == kInvalidPlayerId ||
        second_player_id == kInvalidPlayerId ||
        first_player_id == second_player_id ||
        coordinator_transport.remotes.size() != 2) {
        std::cerr << "network packet smoke failed: multi-peer join allocation failed\n";
        return false;
    }
    const PlayerSlot* const first_slot = coordinator.players.Find(first_player_id);
    const PlayerSlot* const second_slot = coordinator.players.Find(second_player_id);
    if (first_slot == nullptr ||
        second_slot == nullptr ||
        !first_slot->connected ||
        !second_slot->connected ||
        first_slot->connection_kind != PlayerConnectionKind::Remote ||
        second_slot->connection_kind != PlayerConnectionKind::Remote ||
        !first_slot->entity_vid.has_value() ||
        !second_slot->entity_vid.has_value()) {
        std::cerr << "network packet smoke failed: multi-peer coordinator slots are invalid\n";
        return false;
    }

    std::cout << "network packet smoke multi-peer join ok\n";
    return true;
}

bool RunMultiPeerDurableHistoryRetentionPacketSmoke(Graphics& graphics) {
    constexpr std::uint32_t seed = 98769;
    State coordinator = State::New();
    if (!LoadQuestStage(coordinator, "classic", "classic_mines_1", false, seed)) {
        std::cerr << "network packet smoke failed: could not load multi-peer retention stage\n";
        return false;
    }
    ConfigureProtocolSmokeCoordinator(coordinator);
    coordinator.net_session.quest_id = coordinator.stage.quest_id;
    coordinator.net_session.quest_stage_id = coordinator.stage.quest_stage_id;
    coordinator.net_session.stage_seed = seed;

    network::NetTransportRuntime coordinator_transport = network::NetTransportRuntime::New();
    coordinator_transport.capture_outgoing_packets = true;
    const network::NetEndpoint first_endpoint{.address = "127.0.0.1", .port = 43333};
    const network::NetEndpoint second_endpoint{.address = "127.0.0.1", .port = 43334};

    const auto join_one_peer = [&](const network::NetEndpoint& endpoint, const char* name) -> bool {
        network::JoinRequestPacket request;
        request.local_player_count = 1;
        network::WriteFixedString(name, request.display_name);
        network::UdpPacket packet;
        packet.endpoint = endpoint;
        network::HandleJoinRequestAsCoordinator(
            coordinator,
            graphics,
            coordinator_transport,
            packet,
            request
        );
        const std::vector<network::UdpPacket> accept_packets =
            TakeCapturedPackets(coordinator_transport);
        return accept_packets.size() == 1;
    };

    if (!join_one_peer(first_endpoint, "RetainA") ||
        !join_one_peer(second_endpoint, "RetainB") ||
        coordinator_transport.remotes.size() != 2) {
        std::cerr << "network packet smoke failed: multi-peer retention join failed\n";
        return false;
    }

    coordinator.net_session.ordered_events.clear();
    const IVec2 tile_pos = IVec2::New(6, 6);
    coordinator.stage.SetTile(tile_pos, Tile::CaveDirt);
    if (!world_ops::SetForegroundTile(coordinator, tile_pos, Tile::CaveBlock)) {
        std::cerr << "network packet smoke failed: multi-peer retention could not mutate tile\n";
        return false;
    }
    if (coordinator.net_session.ordered_events.size() != 1 ||
        coordinator.net_session.ordered_events[0].header.coordinator_order == 0) {
        std::cerr << "network packet smoke failed: multi-peer retention queued no ordered event\n";
        return false;
    }
    const std::uint64_t event_order =
        coordinator.net_session.ordered_events[0].header.coordinator_order;

    coordinator_transport.remotes[0].highest_acked_coordinator_order = event_order;
    coordinator_transport.remotes[1].highest_acked_coordinator_order = 0;
    network::PruneAckedOrderedEvents(coordinator, coordinator_transport);
    if (coordinator.net_session.ordered_events.empty()) {
        std::cerr << "network packet smoke failed: multi-peer retention pruned before all peers acked\n";
        return false;
    }

    coordinator_transport.remotes[1].highest_acked_coordinator_order = event_order;
    network::PruneAckedOrderedEvents(coordinator, coordinator_transport);
    if (!coordinator.net_session.ordered_events.empty()) {
        std::cerr << "network packet smoke failed: multi-peer retention kept fully acked event\n";
        return false;
    }

    std::cout << "network packet smoke multi-peer durable history retention ok\n";
    return true;
}

bool IsEntityDeadOrInactive(const State& state, VID vid) {
    const Entity* const entity = state.entity_manager.GetEntity(vid);
    return entity == nullptr ||
           !entity->active ||
           entity->health == 0 ||
           entity->condition == EntityCondition::Dead;
}

} // namespace

bool CheckNetworkPacketSmoke() {
    try {
        Graphics graphics;
        InitNetworkSmokeRuntimeTables(graphics);
        Audio audio;

        if (!RunLateJoinSnapshotPacketSmoke(graphics, audio)) {
            return false;
        }
        if (!RunReconnectPlayerIdPacketSmoke(graphics)) {
            return false;
        }
        if (!RunReconnectAfterWorldMutationPacketSmoke(graphics, audio)) {
            return false;
        }
        if (!RunFreshReconnectPlayerIdPacketSmoke(graphics)) {
            return false;
        }
        if (!RunReconnectSpawnModesPacketSmoke(graphics)) {
            return false;
        }
        if (!RunMultiLocalJoinPacketSmoke(graphics)) {
            return false;
        }
        if (!RunMultiPeerJoinPacketSmoke(graphics)) {
            return false;
        }
        if (!RunMultiPeerDurableHistoryRetentionPacketSmoke(graphics)) {
            return false;
        }

        constexpr std::uint32_t seed = 12345;
        State coordinator = State::New();
        State peer = State::New();
        ConfigureProtocolSmokeCoordinator(coordinator);
        ConfigureProtocolSmokePeer(peer);

        if (!LoadQuestStage(coordinator, "classic", "classic_mines_1", false, seed) ||
            !LoadQuestStage(peer, "classic", "classic_mines_1", false, seed)) {
            std::cerr << "network packet smoke failed: could not load test stages\n";
            return false;
        }
        ConfigureProtocolSmokeCoordinator(coordinator);
        ConfigureProtocolSmokePeer(peer);
        LinkMatchingEntitiesForActionSmoke(coordinator, peer);

        network::NetTransportRuntime coordinator_transport = network::NetTransportRuntime::New();
        network::NetTransportRuntime peer_transport = network::NetTransportRuntime::New();
        coordinator_transport.capture_outgoing_packets = true;
        peer_transport.capture_outgoing_packets = true;
        const network::NetEndpoint coordinator_endpoint{
            .address = "127.0.0.1",
            .port = 43101,
        };
        const network::NetEndpoint peer_endpoint{
            .address = "127.0.0.1",
            .port = 43102,
        };
        peer_transport.coordinator_endpoint = coordinator_endpoint;
        coordinator_transport.remotes.push_back(network::NetRemoteEndpoint{
            .player_ids = {2},
            .endpoint = peer_endpoint,
            .last_heard_frame = 0,
            .highest_acked_coordinator_order = 0,
        });

        if (!CompareProtocolSmokeStates(coordinator, peer, "packet after load")) {
            return false;
        }
        const PacketDeliveryPlan reversed_duplicate_delivery{
            .reverse_order = true,
            .duplicate_each_packet = true,
        };

        const Entity* const coordinator_source = FindFirstPlayerLikeEntity(coordinator);
        const Entity* const peer_source = FindFirstPlayerLikeEntity(peer);
        if (coordinator_source == nullptr || peer_source == nullptr) {
            std::cerr << "network packet smoke failed: missing player-like source entity\n";
            return false;
        }
        const VID coordinator_source_vid = coordinator_source->vid;
        const VID peer_source_vid = peer_source->vid;
        const Vec2 coordinator_source_pos = coordinator_source->pos;

        const IVec2 snapshot_tile_pos = IVec2::New(10, 10);
        coordinator.stage.SetTile(snapshot_tile_pos, Tile::CaveBlock);
        coordinator.stage.SetTileRotation(snapshot_tile_pos, kTileRotation90);
        coordinator.stage.SetBackwallTile(snapshot_tile_pos, Tile::Air);
        coordinator.stage.SetFluidTile(IVec2::New(11, 10), Tile::WaterSwim);
        Entity* snapshot_gold = world_ops::SpawnEntity(
            coordinator,
            EntityType::GoldStack,
            [coordinator_source_pos](Entity& entity) {
                entity.pos = coordinator_source_pos + Vec2::New(24.0F, -8.0F);
                entity.vel = Vec2::New(0.0F, 0.0F);
                entity.acc = Vec2::New(0.0F, 0.0F);
            }
        );
        if (snapshot_gold == nullptr) {
            std::cerr << "network packet smoke failed: coordinator could not create snapshot gold\n";
            return false;
        }
        coordinator.net_session.ordered_events.clear();
        peer.net_session.next_expected_coordinator_order = coordinator.net_session.next_coordinator_order;
        network::EnqueueWorldSnapshotEvents(coordinator);
        if (!DeliverCoordinatorPacketsToPeer(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                graphics,
                audio,
                "packet world snapshot bootstrap"
            )) {
            return false;
        }
        if (!CheckSnapshotFingerprint(coordinator, peer, "world snapshot bootstrap")) {
            return false;
        }
        if (const std::optional<VID> peer_snapshot_gold_vid =
                FindPeerEntityForCoordinatorEntity(coordinator, peer, snapshot_gold->vid)) {
            if (Entity* const peer_gold = peer.entity_manager.GetEntityMut(*peer_snapshot_gold_vid)) {
                peer_gold->pos = peer_gold->pos + Vec2::New(48.0F, 32.0F);
                peer_gold->vel = Vec2::New(7.0F, -4.0F);
            }
        }
        peer.stage.SetTile(snapshot_tile_pos, Tile::Air);
        peer.stage.SetTileRotation(snapshot_tile_pos, kTileRotation270);
        peer.stage.SetBackwallTile(snapshot_tile_pos, Tile::CaveDirt);
        peer.stage.SetFluidTile(IVec2::New(11, 10), Tile::Air);
        peer.frame += 99;
        peer.stage_frame += 77;
        peer.depth += 3;
        peer.points += 44;
        peer.deaths += 2;
        peer.sac_altar_favor += 11;
        peer.sac_altar_reward_tier += 1;
        peer.game_over = !coordinator.game_over;
        peer.win = !coordinator.win;
        peer.stage.gravity = 9.0F;
        peer.stage.generation_seed = 777U;
        peer.stage.quest_level_number = -44;
        peer.stage.border.left.tile = Tile::Air;
        peer.stage.border.right.tile = Tile::Air;
        peer.stage.border.top.tile = Tile::Air;
        peer.stage.border.bottom.tile = Tile::Air;
        peer.stage.border.wrap_x = !coordinator.stage.border.wrap_x;
        peer.stage.border.wrap_y = !coordinator.stage.border.wrap_y;
        peer.stage.border.void_death_y = 123;
        peer.stage.camera_clamp_enabled = !coordinator.stage.camera_clamp_enabled;
        peer.stage.wrap_transform_active = !coordinator.stage.wrap_transform_active;
        peer.stage.wrap_padding_tiles = coordinator.stage.wrap_padding_tiles + 3U;
        peer.stage.wrap_core_origin_tiles = UVec2::New(9, 8);
        peer.stage.wrap_core_size_tiles = UVec2::New(7, 6);
        if (Entity* const peer_source_entity = peer.entity_manager.GetEntityMut(peer_source_vid)) {
            peer_source_entity->pos = peer_source_entity->pos + Vec2::New(-32.0F, 24.0F);
            peer_source_entity->vel = Vec2::New(-5.0F, 3.0F);
            peer_source_entity->health = 123;
            peer_source_entity->money = 9999U;
            peer_source_entity->stage_exit_id = 42;
            peer_source_entity->holding = !peer_source_entity->holding;
            peer_source_entity->render_enabled = !peer_source_entity->render_enabled;
            peer_source_entity->damage_vulnerability = DamageVulnerability::ExplosionOnly;
        }
        if (!coordinator_transport.remotes.empty()) {
            const std::uint64_t stale_next_order =
                std::max<std::uint64_t>(peer.net_session.next_expected_coordinator_order, 1);
            coordinator.net_session.ordered_events.clear();
            coordinator.net_session.next_coordinator_order = stale_next_order + 64;
            peer.net_session.highest_applied_coordinator_order = stale_next_order - 1;
            peer.net_session.next_expected_coordinator_order = stale_next_order;
            coordinator_transport.remotes[0].highest_acked_coordinator_order = stale_next_order - 1;
            coordinator_transport.remotes[0].pending_resync_start_order = 0;
            (void)world_ops::SetForegroundTile(
                coordinator,
                IVec2::New(12, 10),
                Tile::TempleBlock
            );
        }
        if (!DeliverCoordinatorPacketsToPeer(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                graphics,
                audio,
                "packet force world snapshot resync after retained gap"
            )) {
            return false;
        }
        if (!CheckSnapshotFingerprint(coordinator, peer, "force world snapshot resync after retained gap")) {
            return false;
        }

        const IVec2 break_tile_pos = IVec2::New(3, 3);
        (void)world_ops::SetForegroundTile(coordinator, break_tile_pos, Tile::CaveDirt);
        if (!DropCoordinatorPacketsToPeer(
                coordinator,
                coordinator_transport,
                "packet setup break tile intentional loss"
            )) {
            return false;
        }
        if (!DeliverCoordinatorPacketsToPeer(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                graphics,
                audio,
                "packet setup break tile resend after loss"
            )) {
            return false;
        }

        if (!RunPeerActionThroughPacketCoordinator(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                GameplayActionRequested{
                    .kind = GameplayActionKind::BreakTile,
                    .source_vid = peer_source_vid,
                    .tile_pos = break_tile_pos,
                },
                graphics,
                audio,
                "packet break tile"
            )) {
            return false;
        }

        const IVec2 duplicate_break_tile_pos = IVec2::New(4, 3);
        (void)world_ops::SetForegroundTile(coordinator, duplicate_break_tile_pos, Tile::CaveDirt);
        if (!DeliverCoordinatorPacketsToPeer(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                graphics,
                audio,
                "packet setup duplicate request break tile"
            )) {
            return false;
        }
        world_ops::RequestGameplayAction(
            peer,
            GameplayActionRequested{
                .kind = GameplayActionKind::BreakTile,
                .source_vid = peer_source_vid,
                .tile_pos = duplicate_break_tile_pos,
            }
        );
        if (!DeliverPeerPacketsToCoordinator(
                peer,
                coordinator,
                peer_transport,
                coordinator_transport,
                peer_endpoint,
                "packet duplicate peer request",
                PacketDeliveryPlan{.duplicate_each_packet = true}
            )) {
            return false;
        }
        if (coordinator.pending_gameplay_actions.size() != 1) {
            std::cerr << "network packet smoke failed: duplicate peer request queued "
                      << coordinator.pending_gameplay_actions.size()
                      << " coordinator actions instead of 1\n";
            return false;
        }
        world_ops::ProcessPendingGameplayActions(coordinator, graphics, audio);
        if (!DeliverCoordinatorPacketsToPeer(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                graphics,
                audio,
                "packet duplicate peer request result"
            )) {
            return false;
        }

        const IVec2 lost_request_break_tile_pos = IVec2::New(5, 3);
        (void)world_ops::SetForegroundTile(coordinator, lost_request_break_tile_pos, Tile::CaveDirt);
        if (!DeliverCoordinatorPacketsToPeer(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                graphics,
                audio,
                "packet setup lost request break tile"
            )) {
            return false;
        }
        world_ops::RequestGameplayAction(
            peer,
            GameplayActionRequested{
                .kind = GameplayActionKind::BreakTile,
                .source_vid = peer_source_vid,
                .tile_pos = lost_request_break_tile_pos,
            }
        );
        network::SendPendingPeerEventsToCoordinator(peer, peer_transport);
        if (TakeCapturedPackets(peer_transport).empty()) {
            std::cerr << "network packet smoke failed: lost peer request emitted no packet to drop\n";
            return false;
        }
        if (peer.net_session.pending_outbound_events.empty()) {
            std::cerr << "network packet smoke failed: lost peer request was removed before ack\n";
            return false;
        }
        if (!DeliverPeerPacketsToCoordinator(
                peer,
                coordinator,
                peer_transport,
                coordinator_transport,
                peer_endpoint,
                "packet lost peer request retry"
            )) {
            return false;
        }
        if (coordinator.pending_gameplay_actions.size() != 1) {
            std::cerr << "network packet smoke failed: lost request retry queued "
                      << coordinator.pending_gameplay_actions.size()
                      << " coordinator actions instead of 1\n";
            return false;
        }
        world_ops::ProcessPendingGameplayActions(coordinator, graphics, audio);
        if (!DeliverCoordinatorPacketsToPeer(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                graphics,
                audio,
                "packet lost peer request retry result"
            )) {
            return false;
        }

        const IVec2 lost_ack_break_tile_pos = IVec2::New(6, 3);
        (void)world_ops::SetForegroundTile(coordinator, lost_ack_break_tile_pos, Tile::CaveDirt);
        if (!DeliverCoordinatorPacketsToPeer(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                graphics,
                audio,
                "packet setup lost action ack break tile"
            )) {
            return false;
        }
        world_ops::RequestGameplayAction(
            peer,
            GameplayActionRequested{
                .kind = GameplayActionKind::BreakTile,
                .source_vid = peer_source_vid,
                .tile_pos = lost_ack_break_tile_pos,
            }
        );
        if (!DeliverPeerPacketsToCoordinator(
                peer,
                coordinator,
                peer_transport,
                coordinator_transport,
                peer_endpoint,
                "packet lost action ack request"
            )) {
            return false;
        }
        if (coordinator.pending_gameplay_actions.size() != 1) {
            std::cerr << "network packet smoke failed: lost action ack setup queued "
                      << coordinator.pending_gameplay_actions.size()
                      << " coordinator actions instead of 1\n";
            return false;
        }
        world_ops::ProcessPendingGameplayActions(coordinator, graphics, audio);
        if (!DeliverCoordinatorPacketsToPeer(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                graphics,
                audio,
                "packet lost action ack result",
                true,
                PacketDeliveryPlan{.drop_action_ack = true}
            )) {
            return false;
        }
        if (!peer.net_session.pending_outbound_events.empty()) {
            std::cerr << "network packet smoke failed: coordinator result did not implicitly ack "
                         "request after explicit action ack was dropped\n";
            return false;
        }

        world_ops::RequestGameplayAction(
            peer,
            GameplayActionRequested{
                .kind = GameplayActionKind::PickupEntity,
                .source_vid = peer_source_vid,
            }
        );
        if (!DeliverPeerPacketsToCoordinator(
                peer,
                coordinator,
                peer_transport,
                coordinator_transport,
                peer_endpoint,
                "packet rejected action request",
                PacketDeliveryPlan{.duplicate_each_packet = true}
            )) {
            return false;
        }
        if (coordinator.pending_gameplay_actions.size() != 1) {
            std::cerr << "network packet smoke failed: rejected duplicate request queued "
                      << coordinator.pending_gameplay_actions.size()
                      << " coordinator actions instead of 1\n";
            return false;
        }
        world_ops::ProcessPendingGameplayActions(coordinator, graphics, audio);
        if (!coordinator.pending_gameplay_actions.empty()) {
            std::cerr << "network packet smoke failed: rejected action left coordinator work queued\n";
            return false;
        }
        if (!DeliverCoordinatorPacketsToPeer(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                graphics,
                audio,
                "packet rejected action ack"
            )) {
            return false;
        }
        if (!peer.net_session.pending_outbound_events.empty()) {
            std::cerr << "network packet smoke failed: rejected action ack did not clear peer request\n";
            return false;
        }

        Entity* gold = world_ops::SpawnEntity(
            coordinator,
            EntityType::Gold,
            [coordinator_source_pos](Entity& entity) {
                entity.pos = coordinator_source_pos;
                entity.vel = Vec2::New(0.0F, 0.0F);
                entity.acc = Vec2::New(0.0F, 0.0F);
            }
        );
        if (gold == nullptr) {
            std::cerr << "network packet smoke failed: coordinator could not spawn gold\n";
            return false;
        }
        const VID coordinator_gold_vid = gold->vid;
        if (!DeliverCoordinatorPacketsToPeer(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                graphics,
                audio,
                "packet setup collect"
            )) {
            return false;
        }
        const std::optional<VID> peer_gold_vid =
            FindPeerEntityForCoordinatorEntity(coordinator, peer, coordinator_gold_vid);
        if (!peer_gold_vid.has_value()) {
            std::cerr << "network packet smoke failed: peer could not resolve spawned gold\n";
            return false;
        }
        if (!RunPeerActionThroughPacketCoordinator(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                GameplayActionRequested{
                    .kind = GameplayActionKind::CollectEntity,
                    .source_vid = peer_source_vid,
                    .target_vid = *peer_gold_vid,
                },
                graphics,
                audio,
                "packet collect entity"
            )) {
            return false;
        }

        Entity* chest = world_ops::SpawnEntity(
            coordinator,
            EntityType::Chest,
            [coordinator_source_pos](Entity& entity) {
                entity.pos = coordinator_source_pos;
                entity.vel = Vec2::New(0.0F, 0.0F);
                entity.acc = Vec2::New(0.0F, 0.0F);
            }
        );
        if (chest == nullptr) {
            std::cerr << "network packet smoke failed: coordinator could not spawn chest\n";
            return false;
        }
        const VID coordinator_chest_vid = chest->vid;
        if (!DeliverCoordinatorPacketsToPeer(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                graphics,
                audio,
                "packet setup interact chest"
            )) {
            return false;
        }
        const std::optional<VID> peer_chest_vid =
            FindPeerEntityForCoordinatorEntity(coordinator, peer, coordinator_chest_vid);
        if (!peer_chest_vid.has_value()) {
            std::cerr << "network packet smoke failed: peer could not resolve spawned chest\n";
            return false;
        }
        if (!RunPeerActionThroughPacketCoordinator(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                GameplayActionRequested{
                    .kind = GameplayActionKind::InteractEntity,
                    .source_vid = peer_source_vid,
                    .target_vid = *peer_chest_vid,
                },
                graphics,
                audio,
                "packet interact chest"
            )) {
            return false;
        }

        Entity* rock = world_ops::SpawnEntity(
            coordinator,
            EntityType::Rock,
            [coordinator_source_pos](Entity& entity) {
                entity.pos = coordinator_source_pos + Vec2::New(8.0F, 0.0F);
                entity.vel = Vec2::New(0.0F, 0.0F);
                entity.acc = Vec2::New(0.0F, 0.0F);
            }
        );
        if (rock == nullptr) {
            std::cerr << "network packet smoke failed: coordinator could not spawn rock\n";
            return false;
        }
        const VID coordinator_rock_vid = rock->vid;
        if (!DeliverCoordinatorPacketsToPeer(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                graphics,
                audio,
                "packet setup pickup"
            )) {
            return false;
        }
        const std::optional<VID> peer_rock_vid =
            FindPeerEntityForCoordinatorEntity(coordinator, peer, coordinator_rock_vid);
        if (!peer_rock_vid.has_value()) {
            std::cerr << "network packet smoke failed: peer could not resolve spawned rock\n";
            return false;
        }
        if (!RunPeerActionThroughPacketCoordinator(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                GameplayActionRequested{
                    .kind = GameplayActionKind::PickupEntity,
                    .source_vid = peer_source_vid,
                    .target_vid = *peer_rock_vid,
                },
                graphics,
                audio,
                "packet pickup entity"
            )) {
            return false;
        }
        if (!RunPeerActionThroughPacketCoordinator(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                GameplayActionRequested{
                    .kind = GameplayActionKind::ThrowEntity,
                    .source_vid = peer_source_vid,
                    .target_vid = *peer_rock_vid,
                    .velocity = Vec2::New(2.0F, -3.0F),
                },
                graphics,
                audio,
                "packet throw entity"
            )) {
            return false;
        }

        Entity* cape = world_ops::SpawnEntity(
            coordinator,
            EntityType::Cape,
            [coordinator_source_pos](Entity& entity) {
                entity.pos = coordinator_source_pos + Vec2::New(8.0F, 0.0F);
                entity.vel = Vec2::New(0.0F, 0.0F);
                entity.acc = Vec2::New(0.0F, 0.0F);
            }
        );
        if (cape == nullptr) {
            std::cerr << "network packet smoke failed: coordinator could not spawn cape\n";
            return false;
        }
        const VID coordinator_cape_vid = cape->vid;
        if (!DeliverCoordinatorPacketsToPeer(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                graphics,
                audio,
                "packet setup cape"
            )) {
            return false;
        }
        const std::optional<VID> peer_cape_vid =
            FindPeerEntityForCoordinatorEntity(coordinator, peer, coordinator_cape_vid);
        if (!peer_cape_vid.has_value()) {
            std::cerr << "network packet smoke failed: peer could not resolve spawned cape\n";
            return false;
        }
        if (!RunPeerActionThroughPacketCoordinator(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                GameplayActionRequested{
                    .kind = GameplayActionKind::PickupEntity,
                    .source_vid = peer_source_vid,
                    .target_vid = *peer_cape_vid,
                },
                graphics,
                audio,
                "packet pickup cape"
            )) {
            return false;
        }
        if (!RunPeerActionThroughPacketCoordinator(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                GameplayActionRequested{
                    .kind = GameplayActionKind::PutHeldEntityOnBack,
                    .source_vid = peer_source_vid,
                    .target_vid = *peer_cape_vid,
                },
                graphics,
                audio,
                "packet put cape on back"
            )) {
            return false;
        }
        if (!RunPeerActionThroughPacketCoordinator(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                GameplayActionRequested{
                    .kind = GameplayActionKind::TakeOffBackEntity,
                    .source_vid = peer_source_vid,
                    .target_vid = *peer_cape_vid,
                },
                graphics,
                audio,
                "packet take off cape"
            )) {
            return false;
        }
        Entity* drop_item = world_ops::SpawnEntity(
            coordinator,
            EntityType::Rock,
            [coordinator_source_pos](Entity& entity) {
                entity.pos = coordinator_source_pos + Vec2::New(8.0F, 0.0F);
                entity.vel = Vec2::New(0.0F, 0.0F);
                entity.acc = Vec2::New(0.0F, 0.0F);
            }
        );
        if (drop_item == nullptr) {
            std::cerr << "network packet smoke failed: coordinator could not spawn drop item\n";
            return false;
        }
        const VID coordinator_drop_item_vid = drop_item->vid;
        if (!DeliverCoordinatorPacketsToPeer(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                graphics,
                audio,
                "packet setup drop"
            )) {
            return false;
        }
        const std::optional<VID> peer_drop_item_vid =
            FindPeerEntityForCoordinatorEntity(coordinator, peer, coordinator_drop_item_vid);
        if (!peer_drop_item_vid.has_value()) {
            std::cerr << "network packet smoke failed: peer could not resolve drop item\n";
            return false;
        }
        if (!RunPeerActionThroughPacketCoordinator(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                GameplayActionRequested{
                    .kind = GameplayActionKind::PickupEntity,
                    .source_vid = peer_source_vid,
                    .target_vid = *peer_drop_item_vid,
                },
                graphics,
                audio,
                "packet pickup drop item"
            )) {
            return false;
        }
        if (!RunPeerActionThroughPacketCoordinator(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                GameplayActionRequested{
                    .kind = GameplayActionKind::DropEntity,
                    .source_vid = peer_source_vid,
                    .target_vid = *peer_drop_item_vid,
                },
                graphics,
                audio,
                "packet drop entity"
            )) {
            return false;
        }

        Entity* block = world_ops::SpawnEntity(
            coordinator,
            EntityType::Block,
            [coordinator_source_pos](Entity& entity) {
                entity.pos = coordinator_source_pos + Vec2::New(5.0F, 0.0F);
                entity.vel = Vec2::New(0.0F, 0.0F);
                entity.acc = Vec2::New(0.0F, 0.0F);
            }
        );
        if (block == nullptr) {
            std::cerr << "network packet smoke failed: coordinator could not spawn block\n";
            return false;
        }
        const VID coordinator_block_vid = block->vid;
        if (!DeliverCoordinatorPacketsToPeer(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                graphics,
                audio,
                "packet setup push"
            )) {
            return false;
        }
        const std::optional<VID> peer_block_vid =
            FindPeerEntityForCoordinatorEntity(coordinator, peer, coordinator_block_vid);
        if (!peer_block_vid.has_value()) {
            std::cerr << "network packet smoke failed: peer could not resolve spawned block\n";
            return false;
        }
        if (Entity* const source = coordinator.entity_manager.GetEntityMut(coordinator_source_vid)) {
            source->grounded = true;
        }
        if (Entity* const source = peer.entity_manager.GetEntityMut(peer_source_vid)) {
            source->grounded = true;
        }
        if (!RunPeerActionThroughPacketCoordinator(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                GameplayActionRequested{
                    .kind = GameplayActionKind::PushEntity,
                    .source_vid = peer_source_vid,
                    .target_vid = *peer_block_vid,
                    .velocity = Vec2::New(0.5F, 0.0F),
                },
                graphics,
                audio,
                "packet push block"
            )) {
            return false;
        }

        Entity* damage_target = world_ops::SpawnEntity(
            coordinator,
            EntityType::Caveman,
            [coordinator_source_pos](Entity& entity) {
                entity.pos = coordinator_source_pos + Vec2::New(20.0F, 0.0F);
                entity.vel = Vec2::New(0.0F, 0.0F);
                entity.acc = Vec2::New(0.0F, 0.0F);
            }
        );
        if (damage_target == nullptr) {
            std::cerr << "network packet smoke failed: coordinator could not spawn damage target\n";
            return false;
        }
        const VID coordinator_damage_target_vid = damage_target->vid;
        if (!DeliverCoordinatorPacketsToPeer(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                graphics,
                audio,
                "packet setup damage"
            )) {
            return false;
        }
        const std::optional<VID> peer_damage_target_vid =
            FindPeerEntityForCoordinatorEntity(coordinator, peer, coordinator_damage_target_vid);
        if (!peer_damage_target_vid.has_value()) {
            std::cerr << "network packet smoke failed: peer could not resolve damage target\n";
            return false;
        }
        if (!RunPeerActionThroughPacketCoordinator(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                GameplayActionRequested{
                    .kind = GameplayActionKind::DamageEntity,
                    .source_vid = peer_source_vid,
                    .target_vid = *peer_damage_target_vid,
                    .damage_type = DamageType::Attack,
                    .amount = 1,
                },
                graphics,
                audio,
                "packet damage entity"
            )) {
            return false;
        }

        Entity* box = world_ops::SpawnEntity(
            coordinator,
            EntityType::Box,
            [coordinator_source_pos](Entity& entity) {
                entity.pos = coordinator_source_pos + Vec2::New(34.0F, 0.0F);
                entity.vel = Vec2::New(0.0F, 0.0F);
                entity.acc = Vec2::New(0.0F, 0.0F);
            }
        );
        if (box == nullptr) {
            std::cerr << "network packet smoke failed: coordinator could not spawn box\n";
            return false;
        }
        const VID coordinator_box_vid = box->vid;
        if (!DeliverCoordinatorPacketsToPeer(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                graphics,
                audio,
                "packet setup box break"
            )) {
            return false;
        }
        const std::optional<VID> peer_box_vid =
            FindPeerEntityForCoordinatorEntity(coordinator, peer, coordinator_box_vid);
        if (!peer_box_vid.has_value()) {
            std::cerr << "network packet smoke failed: peer could not resolve box\n";
            return false;
        }
        if (!RunPeerActionThroughPacketCoordinator(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                GameplayActionRequested{
                    .kind = GameplayActionKind::DamageEntity,
                    .source_vid = peer_source_vid,
                    .target_vid = *peer_box_vid,
                    .damage_type = DamageType::Attack,
                    .amount = 1,
                },
                graphics,
                audio,
                "packet break box"
            )) {
            return false;
        }
        if (!IsEntityDeadOrInactive(coordinator, coordinator_box_vid) ||
            !IsEntityDeadOrInactive(peer, *peer_box_vid)) {
            std::cerr << "network packet smoke failed: box break did not converge dead state\n";
            return false;
        }

        Entity* pot = world_ops::SpawnEntity(
            coordinator,
            EntityType::Pot,
            [coordinator_source_pos](Entity& entity) {
                entity.pos = coordinator_source_pos + Vec2::New(40.0F, 0.0F);
                entity.vel = Vec2::New(0.0F, 0.0F);
                entity.acc = Vec2::New(0.0F, 0.0F);
            }
        );
        if (pot == nullptr) {
            std::cerr << "network packet smoke failed: coordinator could not spawn pot\n";
            return false;
        }
        const VID coordinator_pot_vid = pot->vid;
        if (!DeliverCoordinatorPacketsToPeer(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                graphics,
                audio,
                "packet setup pot break"
            )) {
            return false;
        }
        const std::optional<VID> peer_pot_vid =
            FindPeerEntityForCoordinatorEntity(coordinator, peer, coordinator_pot_vid);
        if (!peer_pot_vid.has_value()) {
            std::cerr << "network packet smoke failed: peer could not resolve pot\n";
            return false;
        }
        if (!RunPeerActionThroughPacketCoordinator(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                GameplayActionRequested{
                    .kind = GameplayActionKind::DamageEntity,
                    .source_vid = peer_source_vid,
                    .target_vid = *peer_pot_vid,
                    .damage_type = DamageType::Attack,
                    .amount = 1,
                },
                graphics,
                audio,
                "packet break pot"
            )) {
            return false;
        }
        if (!IsEntityDeadOrInactive(coordinator, coordinator_pot_vid) ||
            !IsEntityDeadOrInactive(peer, *peer_pot_vid)) {
            std::cerr << "network packet smoke failed: pot break did not converge dead state\n";
            return false;
        }

        Entity* hit_target = world_ops::SpawnEntity(
            coordinator,
            EntityType::Caveman,
            [coordinator_source_pos](Entity& entity) {
                entity.pos = coordinator_source_pos + Vec2::New(28.0F, 0.0F);
                entity.vel = Vec2::New(0.0F, 0.0F);
                entity.acc = Vec2::New(0.0F, 0.0F);
            }
        );
        if (hit_target == nullptr) {
            std::cerr << "network packet smoke failed: coordinator could not spawn hit target\n";
            return false;
        }
        const VID coordinator_hit_target_vid = hit_target->vid;
        if (!DeliverCoordinatorPacketsToPeer(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                graphics,
                audio,
                "packet setup hit"
            )) {
            return false;
        }
        const std::optional<VID> peer_hit_target_vid =
            FindPeerEntityForCoordinatorEntity(coordinator, peer, coordinator_hit_target_vid);
        if (!peer_hit_target_vid.has_value()) {
            std::cerr << "network packet smoke failed: peer could not resolve hit target\n";
            return false;
        }
        if (!RunPeerActionThroughPacketCoordinator(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                GameplayActionRequested{
                    .kind = GameplayActionKind::HitEntity,
                    .source_vid = peer_source_vid,
                    .target_vid = *peer_hit_target_vid,
                    .velocity = Vec2::New(1.5F, -1.0F),
                    .damage_type = DamageType::Attack,
                    .projectile_contact_damage_type = DamageType::Attack,
                    .amount = 1,
                    .projectile_contact_damage_amount = 1,
                    .thrown_immunity_timer = 12,
                    .projectile_contact_duration = 20,
                    .clear_velocity = true,
                    .clear_acceleration = true,
                },
                graphics,
                audio,
                "packet hit entity",
                reversed_duplicate_delivery
            )) {
            return false;
        }

        const std::optional<std::size_t> peer_tool_slot =
            FindFirstUsableToolSlot(peer, peer_source_vid);
        if (!peer_tool_slot.has_value()) {
            std::cerr << "network packet smoke failed: peer has no usable tool slot\n";
            return false;
        }
        if (!RunPeerActionThroughPacketCoordinator(
                coordinator,
                peer,
                coordinator_transport,
                peer_transport,
                peer_endpoint,
                GameplayActionRequested{
                    .kind = GameplayActionKind::UseTool,
                    .source_vid = peer_source_vid,
                    .velocity = Vec2::New(4.0F, -4.0F),
                    .param_a = static_cast<std::uint32_t>(*peer_tool_slot),
                },
                graphics,
                audio,
                "packet use tool"
            )) {
            return false;
        }

        std::cout << "network packet smoke ok\n";
        return true;
    } catch (const std::exception& e) {
        std::cerr << "network packet smoke failed: " << e.what() << '\n';
        return false;
    }
}

} // namespace splonks

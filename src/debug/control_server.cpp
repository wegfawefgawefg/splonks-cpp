#include "debug/control_server.hpp"

#include "entity/archetype.hpp"
#include "network/net_transport.hpp"
#include "state.hpp"
#include "tile.hpp"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifndef _WIN32
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace splonks::debug {
namespace {

#ifndef _WIN32

bool SetNonBlocking(int fd, std::string* error_out) {
    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        if (error_out != nullptr) {
            *error_out = std::string("fcntl(F_GETFL) failed: ") + std::strerror(errno);
        }
        return false;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        if (error_out != nullptr) {
            *error_out = std::string("fcntl(F_SETFL) failed: ") + std::strerror(errno);
        }
        return false;
    }
    return true;
}

void CloseFd(int& fd) {
    if (fd >= 0) {
        close(fd);
        fd = -1;
    }
}

#endif

std::string JsonEscape(std::string_view text) {
    std::ostringstream out;
    for (const char c : text) {
        switch (c) {
        case '\\':
            out << "\\\\";
            break;
        case '"':
            out << "\\\"";
            break;
        case '\n':
            out << "\\n";
            break;
        case '\r':
            out << "\\r";
            break;
        case '\t':
            out << "\\t";
            break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                    << static_cast<int>(static_cast<unsigned char>(c)) << std::dec;
            } else {
                out << c;
            }
            break;
        }
    }
    return out.str();
}

std::string JsonString(std::string_view text) {
    return "\"" + JsonEscape(text) + "\"";
}

const char* NetRoleName(network::NetRole role) {
    switch (role) {
    case network::NetRole::Offline:
        return "offline";
    case network::NetRole::Coordinator:
        return "coordinator";
    case network::NetRole::Peer:
        return "peer";
    }
    return "unknown";
}

const char* EntityConditionName(EntityCondition condition) {
    switch (condition) {
    case EntityCondition::Normal:
        return "normal";
    case EntityCondition::Dead:
        return "dead";
    case EntityCondition::Stunned:
        return "stunned";
    }
    return "unknown";
}

const char* AiStateName(EntityAiState state) {
    switch (state) {
    case EntityAiState::Idle:
        return "idle";
    case EntityAiState::Disturbed:
        return "disturbed";
    case EntityAiState::Patrolling:
        return "patrolling";
    case EntityAiState::Pursuing:
        return "pursuing";
    case EntityAiState::Returning:
        return "returning";
    }
    return "unknown";
}

const char* ConnectionKindName(PlayerConnectionKind kind) {
    switch (kind) {
    case PlayerConnectionKind::Local:
        return "local";
    case PlayerConnectionKind::Remote:
        return "remote";
    }
    return "unknown";
}

void WriteVec2(std::ostringstream& out, const Vec2& value) {
    out << "{\"x\":" << value.x << ",\"y\":" << value.y << "}";
}

const char* AttachmentModeName(AttachmentMode mode) {
    switch (mode) {
    case AttachmentMode::None:
        return "none";
    case AttachmentMode::Held:
        return "held";
    case AttachmentMode::Back:
        return "back";
    }
    return "unknown";
}

void WriteOptionalVid(std::ostringstream& out, const std::optional<VID>& vid) {
    if (!vid.has_value()) {
        out << "null";
        return;
    }
    out << "{\"id\":" << vid->id << ",\"version\":" << vid->version << "}";
}

float Vec2Distance(float ax, float ay, float bx, float by) {
    const float dx = ax - bx;
    const float dy = ay - by;
    return std::sqrt(dx * dx + dy * dy);
}

void WriteNetTargetDiagnostics(
    std::ostringstream& out,
    const State& state,
    const network::NetRemotePlayerTarget& target,
    float snap_distance
) {
    const PlayerSlot* const slot = state.players.Find(target.player_id);
    out << ",\"slot\":";
    if (slot == nullptr) {
        out << "null";
    } else {
        out << "{\"connection\":" << JsonString(ConnectionKindName(slot->connection_kind))
            << ",\"connected\":" << (slot->connected ? "true" : "false")
            << ",\"primary_local\":" << (slot->primary_local ? "true" : "false")
            << ",\"entity\":";
        WriteOptionalVid(out, slot->entity_vid);
        out << "}";
    }

    const Entity* entity = nullptr;
    if (slot != nullptr && slot->entity_vid.has_value()) {
        entity = state.entity_manager.GetEntity(*slot->entity_vid);
    }

    const std::uint64_t age_frames =
        state.frame > target.last_received_frame ? state.frame - target.last_received_frame : 0ULL;
    out << ",\"age_frames\":" << age_frames;
    out << ",\"target_body\":{"
        << "\"condition\":" << JsonString(EntityConditionName(static_cast<EntityCondition>(target.condition)))
        << ",\"grounded\":" << (target.grounded != 0 ? "true" : "false")
        << ",\"health\":" << target.health
        << ",\"fall_timer\":" << target.fall_timer
        << ",\"coyote_time\":" << target.coyote_time
        << ",\"stun_timer\":" << target.stun_timer
        << ",\"animation_id\":" << target.animation_id
        << ",\"animation_frame\":" << target.animation_frame
        << "}";

    out << ",\"local_body\":";
    if (entity == nullptr) {
        out << "null";
        return;
    }

    const float pos_dist = Vec2Distance(entity->pos.x, entity->pos.y, target.pos_x, target.pos_y);
    const float vel_dist = Vec2Distance(entity->vel.x, entity->vel.y, target.vel_x, target.vel_y);
    out << "{\"condition\":" << JsonString(EntityConditionName(entity->condition))
        << ",\"grounded\":" << (entity->grounded ? "true" : "false")
        << ",\"health\":" << entity->health
        << ",\"fall_timer\":" << entity->fall_timer
        << ",\"coyote_time\":" << entity->coyote_time
        << ",\"stun_timer\":" << entity->stun_timer
        << ",\"animation_id\":" << entity->frame_data_animator.animation_id
        << ",\"animation_frame\":" << entity->frame_data_animator.current_frame
        << "}";
    out << ",\"delta\":{\"pos\":{\"x\":" << target.pos_x - entity->pos.x
        << ",\"y\":" << target.pos_y - entity->pos.y
        << ",\"length\":" << pos_dist
        << "},\"vel\":{\"x\":" << target.vel_x - entity->vel.x
        << ",\"y\":" << target.vel_y - entity->vel.y
        << ",\"length\":" << vel_dist
        << "},\"over_snap_distance\":" << (pos_dist > snap_distance ? "true" : "false")
        << "}";
}

void WriteEntityJson(std::ostringstream& out, const State& state, const Entity& entity) {
    out << "{\"id\":" << entity.vid.id
        << ",\"version\":" << entity.vid.version
        << ",\"type\":" << JsonString(GetEntityTypeName(entity.type_))
        << ",\"active\":" << (entity.active ? "true" : "false")
        << ",\"condition\":" << JsonString(EntityConditionName(entity.condition))
        << ",\"ai\":" << JsonString(AiStateName(entity.ai_state))
        << ",\"grounded\":" << (entity.grounded ? "true" : "false")
        << ",\"health\":" << entity.health
        << ",\"money\":" << entity.money
        << ",\"stun_timer\":" << entity.stun_timer
        << ",\"coyote_time\":" << entity.coyote_time
        << ",\"fall_timer\":" << entity.fall_timer
        << ",\"pos\":";
    WriteVec2(out, entity.pos);
    out << ",\"vel\":";
    WriteVec2(out, entity.vel);
    out << ",\"size\":";
    WriteVec2(out, entity.size);
    out << ",\"holding\":";
    WriteOptionalVid(out, entity.holding_vid);
    out << ",\"held_by\":";
    WriteOptionalVid(out, entity.held_by_vid);
    out << ",\"back\":";
    WriteOptionalVid(out, entity.back_vid);
    out << ",\"entity_a\":";
    WriteOptionalVid(out, entity.entity_a);
    out << ",\"counters\":{\"a\":" << entity.counter_a
        << ",\"b\":" << entity.counter_b
        << ",\"c\":" << entity.counter_c
        << ",\"d\":" << entity.counter_d
        << "}";
    out << ",\"use\":{\"down\":" << (entity.use_state.down ? "true" : "false")
        << ",\"pressed\":" << (entity.use_state.pressed ? "true" : "false")
        << ",\"released\":" << (entity.use_state.released ? "true" : "false")
        << ",\"frames\":" << entity.use_state.frames
        << ",\"source\":" << JsonString(AttachmentModeName(entity.use_state.source))
        << ",\"user\":";
    WriteOptionalVid(out, entity.use_state.user_vid);
    out << "}";
    out << ",\"point_a\":{\"x\":" << entity.point_a.x << ",\"y\":" << entity.point_a.y << "}"
        << ",\"has_physics\":" << (entity.has_physics ? "true" : "false")
        << ",\"can_collide\":" << (entity.can_collide ? "true" : "false")
        << ",\"can_apply_projectile_contact\":"
        << (entity.can_apply_projectile_contact ? "true" : "false")
        << ",\"projectile_contact_timer\":" << entity.projectile_contact_timer
        << ",\"rotation\":" << entity.rotation
        << ",\"facing\":" << JsonString(entity.facing == LeftOrRight::Right ? "right" : "left")
        << ",\"animation\":{\"id\":" << entity.frame_data_animator.animation_id
        << ",\"frame\":" << entity.frame_data_animator.current_frame
        << ",\"time\":" << entity.frame_data_animator.current_time
        << ",\"speed\":" << entity.frame_data_animator.speed
        << ",\"animate\":" << (entity.frame_data_animator.animate ? "true" : "false")
        << ",\"loop\":" << (entity.frame_data_animator.loop ? "true" : "false")
        << ",\"finished\":" << (entity.frame_data_animator.finished ? "true" : "false")
        << "}";
    if (const std::optional<PlayerId> player_id = state.players.FindPlayerIdForEntity(entity.vid)) {
        out << ",\"player_id\":" << *player_id;
    } else {
        out << ",\"player_id\":null";
    }
    if (const std::optional<network::NetEntityId> net_id =
            state.net_session.FindNetEntityId(entity.vid)) {
        out << ",\"net_entity_id\":" << *net_id;
        if (const std::optional<PlayerId> owner = state.net_session.FindEntityOwner(*net_id)) {
            out << ",\"net_owner_player_id\":" << *owner;
        } else {
            out << ",\"net_owner_player_id\":null";
        }
        out << ",\"net_owner\":" << JsonString(
            state.net_session.HasLocalAuthorityForEntity(entity.vid)
                ? "local-authority"
                : "remote-authority"
        );
    } else {
        out << ",\"net_entity_id\":null"
            << ",\"net_owner_player_id\":null"
            << ",\"net_owner\":null";
    }
    out << "}";
}

std::vector<std::string> SplitCommand(std::string_view command) {
    std::istringstream in{std::string(command)};
    std::vector<std::string> parts;
    std::string part;
    while (in >> part) {
        parts.push_back(part);
    }
    return parts;
}

float ParseFloatArg(const std::vector<std::string>& parts, std::size_t index, float fallback) {
    if (index >= parts.size()) {
        return fallback;
    }
    try {
        return std::stof(parts[index]);
    } catch (...) {
        return fallback;
    }
}

int ParseIntArg(const std::vector<std::string>& parts, std::size_t index, int fallback) {
    if (index >= parts.size()) {
        return fallback;
    }
    try {
        return std::stoi(parts[index]);
    } catch (...) {
        return fallback;
    }
}

std::string MakeError(std::string_view message) {
    return "{\"ok\":false,\"error\":" + JsonString(message) + "}\n";
}

std::string HandleStatusCommand(const State& state) {
    const UVec2 dims = state.stage.GetStageDims();
    std::ostringstream out;
    out << "{\"ok\":true,\"cmd\":\"status\""
        << ",\"frame\":" << state.frame
        << ",\"stage_frame\":" << state.stage_frame
        << ",\"mode\":" << static_cast<int>(state.mode)
        << ",\"quest\":" << JsonString(state.stage.quest_id)
        << ",\"stage\":" << JsonString(state.stage.quest_stage_id)
        << ",\"stage_seed\":" << state.net_session.stage_seed
        << ",\"stage_dims\":{\"w\":" << dims.x << ",\"h\":" << dims.y << "}"
        << ",\"players\":" << state.players.slots.size()
        << ",\"entities\":{\"active\":" << state.entity_manager.NumActiveEntities()
        << ",\"capacity\":" << EntityManager::kMaxNumEntities << "}"
        << ",\"net\":{\"role\":" << JsonString(NetRoleName(state.net_session.role))
        << ",\"local_player_id\":" << state.net_session.local_player_id
        << ",\"peers\":" << state.net_session.peers.size() << "}"
        << "}\n";
    return out.str();
}

std::string HandlePerfCommand(const State& state) {
    const PerformanceStats& perf = state.performance_stats;
    std::ostringstream out;
    out << "{\"ok\":true,\"cmd\":\"perf\""
        << ",\"budget_ms\":" << perf.frame_budget_ms
        << ",\"step_ms\":" << perf.step_ms
        << ",\"render_ms\":" << perf.render_ms
        << ",\"imgui_ms\":" << perf.imgui_ms
        << ",\"present_ms\":" << perf.present_ms
        << ",\"frame_total_ms\":" << perf.frame_total_ms
        << ",\"step_smoothed_ms\":" << perf.step_smoothed_ms
        << ",\"render_smoothed_ms\":" << perf.render_smoothed_ms
        << ",\"frame_total_smoothed_ms\":" << perf.frame_total_smoothed_ms
        << ",\"step_peak_ms\":" << perf.step_peak_ms
        << ",\"render_peak_ms\":" << perf.render_peak_ms
        << ",\"frame_total_peak_ms\":" << perf.frame_total_peak_ms
        << "}\n";
    return out.str();
}

std::string HandlePlayersCommand(const State& state) {
    std::ostringstream out;
    out << "{\"ok\":true,\"cmd\":\"players\",\"players\":[";
    bool first = true;
    for (const PlayerSlot& slot : state.players.slots) {
        if (!first) {
            out << ",";
        }
        first = false;
        out << "{\"player_id\":" << slot.player_id
            << ",\"connection\":" << JsonString(ConnectionKindName(slot.connection_kind))
            << ",\"connected\":" << (slot.connected ? "true" : "false")
            << ",\"primary_local\":" << (slot.primary_local ? "true" : "false")
            << ",\"display_name\":" << JsonString(slot.display_name)
            << ",\"entity\":";
        if (slot.entity_vid.has_value()) {
            if (const Entity* const entity = state.entity_manager.GetEntity(*slot.entity_vid)) {
                WriteEntityJson(out, state, *entity);
            } else {
                out << "null";
            }
        } else {
            out << "null";
        }
        out << "}";
    }
    out << "]}\n";
    return out.str();
}

std::string HandleEntityCommand(const State& state, const std::vector<std::string>& parts) {
    if (parts.size() < 2) {
        return MakeError("entity command requires an entity id");
    }
    const int entity_id = ParseIntArg(parts, 1, -1);
    if (entity_id < 0 || static_cast<std::size_t>(entity_id) >= state.entity_manager.entities.size()) {
        return MakeError("entity id is outside the entity array");
    }
    const Entity& entity = state.entity_manager.entities[static_cast<std::size_t>(entity_id)];
    std::ostringstream out;
    out << "{\"ok\":true,\"cmd\":\"entity\",\"entity\":";
    WriteEntityJson(out, state, entity);
    out << "}\n";
    return out.str();
}

std::optional<Vec2> GetPrimaryLocalPlayerCenter(const State& state) {
    const PlayerSlot* const player = state.players.FindPrimaryLocal();
    if (player == nullptr || !player->entity_vid.has_value()) {
        return std::nullopt;
    }
    const Entity* const entity = state.entity_manager.GetEntity(*player->entity_vid);
    if (entity == nullptr) {
        return std::nullopt;
    }
    return entity->GetCenter();
}

std::string HandleEntitiesCommand(const State& state, const std::vector<std::string>& parts) {
    bool near_primary_player = false;
    float radius = 128.0F;
    int limit = 128;
    if (parts.size() >= 2 && parts[1] == "near") {
        near_primary_player = true;
        radius = ParseFloatArg(parts, 2, radius);
        limit = ParseIntArg(parts, 3, limit);
    } else if (parts.size() >= 2) {
        limit = ParseIntArg(parts, 1, limit);
    }
    limit = std::clamp(limit, 1, static_cast<int>(EntityManager::kMaxNumEntities));

    std::optional<Vec2> center;
    if (near_primary_player) {
        center = GetPrimaryLocalPlayerCenter(state);
        if (!center.has_value()) {
            return MakeError("no primary local player entity is available for near query");
        }
    }

    std::ostringstream out;
    int emitted = 0;
    int matching = 0;
    out << "{\"ok\":true,\"cmd\":\"entities\",\"limit\":" << limit << ",\"entities\":[";
    bool first = true;
    for (const Entity& entity : state.entity_manager.entities) {
        if (!entity.active) {
            continue;
        }
        if (center.has_value()) {
            const Vec2 delta = entity.GetCenter() - *center;
            const float dist_sq = delta.x * delta.x + delta.y * delta.y;
            if (dist_sq > radius * radius) {
                continue;
            }
        }
        ++matching;
        if (emitted >= limit) {
            continue;
        }
        if (!first) {
            out << ",";
        }
        first = false;
        WriteEntityJson(out, state, entity);
        ++emitted;
    }
    out << "],\"matching\":" << matching << ",\"emitted\":" << emitted << "}\n";
    return out.str();
}

std::string HandleNetCommand(const State& state) {
    std::ostringstream out;
    out << "{\"ok\":true,\"cmd\":\"net\""
        << ",\"role\":" << JsonString(NetRoleName(state.net_session.role))
        << ",\"local_player_id\":" << state.net_session.local_player_id
        << ",\"coordinator_player_id\":" << state.net_session.coordinator_player_id
        << ",\"stage_instance_id\":" << state.net_session.stage_instance_id
        << ",\"quest\":" << JsonString(state.net_session.quest_id)
        << ",\"stage\":" << JsonString(state.net_session.quest_stage_id)
        << ",\"seed\":" << state.net_session.stage_seed
        << ",\"pending_outbound_events\":" << state.net_session.pending_outbound_events.size()
        << ",\"ordered_events\":" << state.net_session.ordered_events.size()
        << ",\"applied_events\":" << state.net_session.applied_event_ids.size()
        << ",\"next_expected_coordinator_order\":" << state.net_session.next_expected_coordinator_order
        << ",\"highest_applied_coordinator_order\":" << state.net_session.highest_applied_coordinator_order
        << ",\"entity_links\":" << state.net_session.entity_links.size()
        << ",\"peers\":[";
    for (std::size_t i = 0; i < state.net_session.peers.size(); ++i) {
        const network::NetPeerState& peer = state.net_session.peers[i];
        if (i > 0) {
            out << ",";
        }
        out << "{\"player_id\":" << peer.player_id
            << ",\"name\":" << JsonString(peer.display_name)
            << ",\"endpoint\":" << JsonString(peer.endpoint_address + ":" + std::to_string(peer.endpoint_port))
            << ",\"ping_ms\":" << peer.estimated_ping_ms
            << ",\"jitter_ms\":" << peer.jitter_ms << "}";
    }
    out << "],\"transport\":";
    if (state.net_transport) {
        out << "{\"socket_port\":" << state.net_transport->socket.BoundPort()
            << ",\"remotes\":" << state.net_transport->remotes.size()
            << ",\"remote_acks\":[";
        for (std::size_t i = 0; i < state.net_transport->remotes.size(); ++i) {
            const network::NetRemoteEndpoint& remote = state.net_transport->remotes[i];
            if (i > 0) {
                out << ",";
            }
            out << "{\"endpoint\":" << JsonString(remote.endpoint.address + ":" + std::to_string(remote.endpoint.port))
                << ",\"highest_acked_coordinator_order\":" << remote.highest_acked_coordinator_order
                << "}";
        }
        out << "]"
            << ",\"remote_targets\":[";
        for (std::size_t i = 0; i < state.net_transport->remote_player_targets.size(); ++i) {
            const network::NetRemotePlayerTarget& target = state.net_transport->remote_player_targets[i];
            if (i > 0) {
                out << ",";
            }
            out << "{\"player_id\":" << target.player_id
                << ",\"sequence\":" << target.sequence
                << ",\"last_received_frame\":" << target.last_received_frame
                << ",\"pos\":{\"x\":" << target.pos_x << ",\"y\":" << target.pos_y << "}"
                << ",\"vel\":{\"x\":" << target.vel_x << ",\"y\":" << target.vel_y << "}";
            WriteNetTargetDiagnostics(out, state, target, state.net_transport->remote_snap_distance);
            out << "}";
        }
        out << "]}";
    } else {
        out << "null";
    }
    out << "}\n";
    return out.str();
}

std::string HandleTilesCommand(const State& state, const std::vector<std::string>& parts) {
    if (parts.size() < 5) {
        return MakeError("tiles command requires x y w h");
    }
    const int x = ParseIntArg(parts, 1, 0);
    const int y = ParseIntArg(parts, 2, 0);
    const int w = std::clamp(ParseIntArg(parts, 3, 8), 1, 32);
    const int h = std::clamp(ParseIntArg(parts, 4, 8), 1, 32);
    std::ostringstream out;
    out << "{\"ok\":true,\"cmd\":\"tiles\",\"x\":" << x << ",\"y\":" << y
        << ",\"w\":" << w << ",\"h\":" << h << ",\"rows\":[";
    for (int row = 0; row < h; ++row) {
        if (row > 0) {
            out << ",";
        }
        out << "[";
        for (int col = 0; col < w; ++col) {
            if (col > 0) {
                out << ",";
            }
            const int tile_x = x + col;
            const int tile_y = y + row;
            const Tile tile = state.stage.GetTileOrBorder(tile_x, tile_y);
            const bool inside = state.stage.IsTileCoordInside(tile_x, tile_y);
            out << "{\"tile\":" << JsonString(TileToString(tile))
                << ",\"inside\":" << (inside ? "true" : "false");
            if (inside) {
                const auto ux = static_cast<unsigned int>(tile_x);
                const auto uy = static_cast<unsigned int>(tile_y);
                out << ",\"fluid\":" << state.stage.GetFluidAmount(ux, uy)
                    << ",\"fluid_tile\":" << JsonString(TileToString(state.stage.GetFluidTile(ux, uy)));
            }
            out << "}";
        }
        out << "]";
    }
    out << "]}\n";
    return out.str();
}

std::string HandleCommand(const State& state, std::string_view command) {
    const std::vector<std::string> parts = SplitCommand(command);
    if (parts.empty()) {
        return MakeError("empty command");
    }
    const std::string& op = parts[0];
    if (op == "ping") {
        return "{\"ok\":true,\"cmd\":\"ping\",\"pong\":true}\n";
    }
    if (op == "help") {
        return "{\"ok\":true,\"cmd\":\"help\",\"commands\":[\"ping\",\"status\",\"players\",\"entities [limit]\",\"entities near [radius] [limit]\",\"entity <id>\",\"tiles <x> <y> <w> <h>\",\"net\",\"perf\"]}\n";
    }
    if (op == "status") {
        return HandleStatusCommand(state);
    }
    if (op == "players") {
        return HandlePlayersCommand(state);
    }
    if (op == "entities") {
        return HandleEntitiesCommand(state, parts);
    }
    if (op == "entity") {
        return HandleEntityCommand(state, parts);
    }
    if (op == "tiles") {
        return HandleTilesCommand(state, parts);
    }
    if (op == "net") {
        return HandleNetCommand(state);
    }
    if (op == "perf") {
        return HandlePerfCommand(state);
    }
    return MakeError("unknown command");
}

} // namespace

DebugControlServer::~DebugControlServer() {
    Stop();
}

bool DebugControlServer::Start(std::uint16_t port, std::string* error_out) {
    Stop();
#ifdef _WIN32
    if (error_out != nullptr) {
        *error_out = "debug control server is not implemented on Windows";
    }
    (void)port;
    return false;
#else
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        if (error_out != nullptr) {
            *error_out = std::string("socket failed: ") + std::strerror(errno);
        }
        return false;
    }

    int yes = 1;
    (void)setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    if (!SetNonBlocking(listen_fd_, error_out)) {
        Stop();
        return false;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        if (error_out != nullptr) {
            *error_out = std::string("bind 127.0.0.1:") + std::to_string(port) +
                " failed: " + std::strerror(errno);
        }
        Stop();
        return false;
    }
    if (listen(listen_fd_, 8) < 0) {
        if (error_out != nullptr) {
            *error_out = std::string("listen failed: ") + std::strerror(errno);
        }
        Stop();
        return false;
    }
    port_ = port;
    return true;
#endif
}

void DebugControlServer::Stop() {
#ifndef _WIN32
    CloseFd(listen_fd_);
    for (Client& client : clients_) {
        CloseFd(client.fd);
    }
#endif
    clients_.clear();
    port_ = 0;
}

void DebugControlServer::Step(const State& state) {
#ifdef _WIN32
    (void)state;
#else
    if (listen_fd_ < 0) {
        return;
    }

    while (true) {
        sockaddr_in client_address{};
        socklen_t client_address_size = sizeof(client_address);
        int client_fd = accept(
            listen_fd_,
            reinterpret_cast<sockaddr*>(&client_address),
            &client_address_size
        );
        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            break;
        }
        std::string error;
        if (!SetNonBlocking(client_fd, &error)) {
            CloseFd(client_fd);
            continue;
        }
        clients_.push_back(Client{
            .fd = client_fd,
            .read_buffer = {},
            .write_buffer = {},
            .close_after_write = false,
        });
    }

    for (Client& client : clients_) {
        if (client.fd < 0) {
            continue;
        }

        char buffer[1024]{};
        while (!client.close_after_write) {
            const ssize_t received = recv(client.fd, buffer, sizeof(buffer), 0);
            if (received > 0) {
                client.read_buffer.append(buffer, static_cast<std::size_t>(received));
                const std::size_t newline = client.read_buffer.find('\n');
                if (newline != std::string::npos) {
                    const std::string command = client.read_buffer.substr(0, newline);
                    client.write_buffer += HandleCommand(state, command);
                    client.close_after_write = true;
                    break;
                }
                if (client.read_buffer.size() > 4096) {
                    client.write_buffer += MakeError("command line is too long");
                    client.close_after_write = true;
                    break;
                }
                continue;
            }
            if (received == 0) {
                CloseFd(client.fd);
                break;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            CloseFd(client.fd);
            break;
        }

        while (client.fd >= 0 && !client.write_buffer.empty()) {
            const ssize_t sent = send(
                client.fd,
                client.write_buffer.data(),
                client.write_buffer.size(),
                MSG_NOSIGNAL
            );
            if (sent > 0) {
                client.write_buffer.erase(0, static_cast<std::size_t>(sent));
                continue;
            }
            if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                break;
            }
            CloseFd(client.fd);
            break;
        }

        if (client.fd >= 0 && client.close_after_write && client.write_buffer.empty()) {
            CloseFd(client.fd);
        }
    }

    clients_.erase(
        std::remove_if(
            clients_.begin(),
            clients_.end(),
            [](const Client& client) { return client.fd < 0; }
        ),
        clients_.end()
    );
#endif
}

bool DebugControlServer::IsRunning() const {
    return listen_fd_ >= 0;
}

std::uint16_t DebugControlServer::Port() const {
    return port_;
}

} // namespace splonks::debug

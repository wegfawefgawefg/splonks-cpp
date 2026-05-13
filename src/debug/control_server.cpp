#include "debug/control_server.hpp"

#include "ent/spec.hpp"
#include "network/net_fuzzer.hpp"
#include "network/net_lobby.hpp"
#include "network/net_transport.hpp"
#include "state.hpp"
#include "state_fingerprint.hpp"
#include "tile.hpp"

#include <algorithm>
#include <cerrno>
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

std::size_t LocalInputRecordCount(
    const State& state,
    const network::LockstepInputBuffer& buffer
) {
    std::size_t count = 0;
    for (const PlayerSlot& slot : state.players.slots) {
        if (slot.connected && slot.connection_kind == PlayerConnectionKind::Local &&
            slot.player_id != kInvalidPlayerId) {
            count += buffer.RecordCountForPlayer(slot.player_id);
        }
    }
    return count;
}

const char* NetRoleName(network::NetRole role) {
    switch (role) {
    case network::NetRole::Offline:
        return "offline";
    case network::NetRole::Host:
        return "host";
    case network::NetRole::Peer:
        return "peer";
    }
    return "unknown";
}

const char* LockstepDesyncRecoveryModeName(network::LockstepDesyncRecoveryMode mode) {
    switch (mode) {
    case network::LockstepDesyncRecoveryMode::None:
        return "none";
    case network::LockstepDesyncRecoveryMode::PendingRollback:
        return "pending-rollback";
    case network::LockstepDesyncRecoveryMode::RollbackRepaired:
        return "rollback-repaired";
    case network::LockstepDesyncRecoveryMode::SnapshotCatchup:
        return "snapshot-catchup";
    case network::LockstepDesyncRecoveryMode::FatalDesync:
        return "fatal-desync";
    }
    return "unknown";
}

const char* EntConditionName(EntCondition condition) {
    switch (condition) {
    case EntCondition::Normal:
        return "normal";
    case EntCondition::Dead:
        return "dead";
    case EntCondition::Stunned:
        return "stunned";
    }
    return "unknown";
}

const char* AiStateName(EntAiState state) {
    switch (state) {
    case EntAiState::Idle:
        return "idle";
    case EntAiState::Disturbed:
        return "disturbed";
    case EntAiState::Patrolling:
        return "patrolling";
    case EntAiState::Pursuing:
        return "pursuing";
    case EntAiState::Returning:
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

std::string LowerAscii(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (char c : text) {
        if (c >= 'A' && c <= 'Z') {
            result.push_back(static_cast<char>(c - 'A' + 'a'));
        } else {
            result.push_back(c);
        }
    }
    return result;
}

std::optional<network::NetFuzzerConfig> NetFuzzerPresetByName(std::string_view raw_name) {
    const std::string name = LowerAscii(raw_name);
    if (name == "lan") {
        return network::NetFuzzerConfig::LanPreset();
    }
    if (name == "same-house" || name == "samehouse" || name == "house") {
        return network::NetFuzzerConfig::SameHousePreset();
    }
    if (name == "same-city" || name == "samecity" || name == "city") {
        return network::NetFuzzerConfig::SameCityPreset();
    }
    if (name == "same-state" || name == "samestate" || name == "state") {
        return network::NetFuzzerConfig::SameStatePreset();
    }
    if (name == "same-region" || name == "sameregion" || name == "region") {
        return network::NetFuzzerConfig::SameRegionPreset();
    }
    if (name == "tx-ca" || name == "texas-california" || name == "texas-to-california") {
        return network::NetFuzzerConfig::TexasToCaliforniaPreset();
    }
    if (name == "ca-fl" || name == "california-florida" || name == "california-to-florida") {
        return network::NetFuzzerConfig::CaliforniaToFloridaPreset();
    }
    if (name == "us-cross-country" || name == "cross-country" || name == "crosscountry") {
        return network::NetFuzzerConfig::UsCrossCountryPreset();
    }
    if (name == "tx-japan" || name == "japan-texas" || name == "japan-to-texas" ||
        name == "texas-to-japan") {
        return network::NetFuzzerConfig::JapanToTexasPreset();
    }
    if (name == "bad-wifi" || name == "badwifi") {
        return network::NetFuzzerConfig::BadWifiPreset();
    }
    return std::nullopt;
}

void WriteVec2(std::ostringstream& out, const Vec2& value) {
    out << "{\"x\":" << value.x << ",\"y\":" << value.y << "}";
}

const char* AttachModeName(AttachMode mode) {
    switch (mode) {
    case AttachMode::None:
        return "none";
    case AttachMode::Held:
        return "held";
    case AttachMode::Back:
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

void WriteEntJson(std::ostringstream& out, const State& state, const Ent& ent) {
    out << "{\"id\":" << ent.vid.id
        << ",\"version\":" << ent.vid.version
        << ",\"type\":" << JsonString(GetEntTypeName(ent.type_))
        << ",\"active\":" << (ent.active ? "true" : "false")
        << ",\"condition\":" << JsonString(EntConditionName(ent.condition))
        << ",\"ai\":" << JsonString(AiStateName(ent.ai_state))
        << ",\"grounded\":" << (ent.grounded ? "true" : "false")
        << ",\"health\":" << ent.health
        << ",\"money\":" << ent.money
        << ",\"stun_timer\":" << ent.stun_timer
        << ",\"coyote_time\":" << ent.coyote_time
        << ",\"fall_timer\":" << ent.fall_timer
        << ",\"pos\":";
    WriteVec2(out, ent.pos);
    out << ",\"vel\":";
    WriteVec2(out, ent.vel);
    out << ",\"size\":";
    WriteVec2(out, ent.size);
    out << ",\"holding\":";
    WriteOptionalVid(out, ent.holding_vid);
    out << ",\"held_by\":";
    WriteOptionalVid(out, ent.held_by_vid);
    out << ",\"back\":";
    WriteOptionalVid(out, ent.back_vid);
    out << ",\"ent_a\":";
    WriteOptionalVid(out, ent.ent_a);
    out << ",\"counters\":{\"a\":" << ent.counter_a
        << ",\"b\":" << ent.counter_b
        << ",\"c\":" << ent.counter_c
        << ",\"d\":" << ent.counter_d
        << "}";
    out << ",\"use\":{\"down\":" << (ent.use_state.down ? "true" : "false")
        << ",\"pressed\":" << (ent.use_state.pressed ? "true" : "false")
        << ",\"released\":" << (ent.use_state.released ? "true" : "false")
        << ",\"frames\":" << ent.use_state.frames
        << ",\"source\":" << JsonString(AttachModeName(ent.use_state.source))
        << ",\"user\":";
    WriteOptionalVid(out, ent.use_state.user_vid);
    out << "}";
    out << ",\"point_a\":{\"x\":" << ent.point_a.x << ",\"y\":" << ent.point_a.y << "}"
        << ",\"has_physics\":" << (ent.has_physics ? "true" : "false")
        << ",\"can_collide\":" << (ent.can_collide ? "true" : "false")
        << ",\"can_apply_proj_contact\":"
        << (ent.can_apply_proj_contact ? "true" : "false")
        << ",\"proj_contact_timer\":" << ent.proj_contact_timer
        << ",\"rotation\":" << ent.rotation
        << ",\"facing\":" << JsonString(ent.facing == Side::Right ? "right" : "left")
        << ",\"anim\":{\"id\":" << ent.aframe_animator.anim_id
        << ",\"frame\":" << ent.aframe_animator.current_frame
        << ",\"time\":" << ent.aframe_animator.current_time
        << ",\"speed\":" << ent.aframe_animator.speed
        << ",\"animate\":" << (ent.aframe_animator.animate ? "true" : "false")
        << ",\"loop\":" << (ent.aframe_animator.loop ? "true" : "false")
        << ",\"finished\":" << (ent.aframe_animator.finished ? "true" : "false")
        << "}";
    if (const std::optional<PlayerId> player_id = state.players.FindPlayerIdForEnt(ent.vid)) {
        out << ",\"player_id\":" << *player_id;
    } else {
        out << ",\"player_id\":null";
    }
    if (const std::optional<network::NetEntId> net_id =
            state.net_session.FindNetEntId(ent.vid)) {
        out << ",\"net_ent_id\":" << *net_id;
        if (const std::optional<PlayerId> input_owner =
                state.net_session.FindEntInputOwner(*net_id)) {
            out << ",\"input_owner_player_id\":" << *input_owner;
            const PlayerSlot* const slot = state.players.Find(*input_owner);
            out << ",\"input_owner\":" << JsonString(
                slot != nullptr && slot->connection_kind == PlayerConnectionKind::Local
                    ? "local-input"
                    : "remote-input"
            );
        } else {
            out << ",\"input_owner_player_id\":null"
                << ",\"input_owner\":\"shared\"";
        }
    } else {
        out << ",\"net_ent_id\":null"
            << ",\"input_owner_player_id\":null"
            << ",\"input_owner\":null";
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

PlayerId ParsePlayerIdArg(const std::vector<std::string>& parts, std::size_t index, PlayerId fallback) {
    const int parsed = ParseIntArg(parts, index, static_cast<int>(fallback));
    return parsed < 0 ? fallback : static_cast<PlayerId>(parsed);
}

std::string MakeError(std::string_view message) {
    return "{\"ok\":false,\"error\":" + JsonString(message) + "}\n";
}

bool ApplyInputButtonToken(InputFrame& input, std::string_view token) {
    if (token == "left") {
        input.left = true;
    } else if (token == "right") {
        input.right = true;
    } else if (token == "up") {
        input.up = true;
    } else if (token == "down") {
        input.down = true;
    } else if (token == "jump") {
        input.jump = true;
    } else if (token == "run") {
        input.run = true;
    } else if (token == "use") {
        input.use_button = true;
    } else if (token == "equip") {
        input.equip_button = true;
    } else if (token == "pickup" || token == "grab" || token == "drop") {
        input.pick_up_drop = true;
    } else if (token == "stop") {
        input.stop = true;
    } else if (token == "bomb") {
        input.bomb = true;
    } else if (token == "rope") {
        input.rope = true;
    } else if (token == "attack") {
        input.attack = true;
    } else if (token == "buy") {
        input.buy_button = true;
    } else if (token == "emote_up") {
        input.emote_up = true;
    } else if (token == "emote_down") {
        input.emote_down = true;
    } else {
        return token == "none";
    }
    return true;
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
        << ",\"ents\":{\"active\":" << state.ents.NumActiveEnts()
        << ",\"capacity\":" << EntPool::kMaxNumEnts << "}"
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
        << ",\"network_pump_ms\":" << perf.network_pump_ms
        << ",\"lockstep_hash_ms\":" << perf.lockstep_hash_ms
        << ",\"rollback_snapshot_save_ms\":" << perf.rollback_snapshot_save_ms
        << ",\"rollback_snapshot_restore_ms\":" << perf.rollback_snapshot_restore_ms
        << ",\"rollback_replay_ms_this_frame\":" << perf.rollback_replay_ms_this_frame
        << ",\"rollback_replay_frames_this_frame\":" << perf.rollback_replay_frames_this_frame
        << ",\"rollback_replay_ms_per_frame\":" << perf.rollback_replay_ms_per_frame
        << ",\"multiplayer_sim_total_ms\":" << perf.multiplayer_sim_total_ms
        << ",\"rollback_buffer_bytes\":" << perf.rollback_buffer_bytes
        << ",\"render_ms\":" << perf.render_ms
        << ",\"imgui_ms\":" << perf.imgui_ms
        << ",\"present_ms\":" << perf.present_ms
        << ",\"frame_total_ms\":" << perf.frame_total_ms
        << ",\"step_smoothed_ms\":" << perf.step_smoothed_ms
        << ",\"network_pump_smoothed_ms\":" << perf.network_pump_smoothed_ms
        << ",\"lockstep_hash_smoothed_ms\":" << perf.lockstep_hash_smoothed_ms
        << ",\"rollback_replay_smoothed_ms\":" << perf.rollback_replay_smoothed_ms
        << ",\"multiplayer_sim_total_smoothed_ms\":" << perf.multiplayer_sim_total_smoothed_ms
        << ",\"render_smoothed_ms\":" << perf.render_smoothed_ms
        << ",\"frame_total_smoothed_ms\":" << perf.frame_total_smoothed_ms
        << ",\"step_peak_ms\":" << perf.step_peak_ms
        << ",\"network_pump_peak_ms\":" << perf.network_pump_peak_ms
        << ",\"lockstep_hash_peak_ms\":" << perf.lockstep_hash_peak_ms
        << ",\"rollback_replay_peak_ms\":" << perf.rollback_replay_peak_ms
        << ",\"multiplayer_sim_total_peak_ms\":" << perf.multiplayer_sim_total_peak_ms
        << ",\"render_peak_ms\":" << perf.render_peak_ms
        << ",\"frame_total_peak_ms\":" << perf.frame_total_peak_ms
        << "}\n";
    return out.str();
}

std::string HandleInputCommand(State& state, const std::vector<std::string>& parts) {
    if (parts.size() >= 2 && parts[1] == "clear") {
        state.debug_input_override = DebugInputOverrideState{};
        return "{\"ok\":true,\"cmd\":\"input\",\"cleared\":true}\n";
    }
    if (parts.size() >= 2 && parts[1] == "status") {
        std::ostringstream out;
        out << "{\"ok\":true,\"cmd\":\"input\""
            << ",\"active\":" << (state.debug_input_override.active ? "true" : "false")
            << ",\"player_id\":";
        if (state.debug_input_override.player_id == kInvalidPlayerId) {
            out << "null";
        } else {
            out << state.debug_input_override.player_id;
        }
        out << ",\"frames_remaining\":" << state.debug_input_override.frames_remaining
            << "}\n";
        return out.str();
    }

    PlayerId player_id = kInvalidPlayerId;
    std::size_t frames_index = 1;
    if (parts.size() >= 4 && parts[1] == "player") {
        player_id = ParsePlayerIdArg(parts, 2, kInvalidPlayerId);
        frames_index = 3;
    }
    if (parts.size() <= frames_index) {
        return MakeError("input command requires frames and optional buttons");
    }

    const int frames = ParseIntArg(parts, frames_index, 0);
    if (frames <= 0) {
        return MakeError("input frames must be positive");
    }
    if (player_id != kInvalidPlayerId) {
        const PlayerSlot* const slot = state.players.Find(player_id);
        if (slot == nullptr || !slot->connected ||
            slot->connection_kind != PlayerConnectionKind::Local) {
            return MakeError("input target must be a connected local player");
        }
    } else if (state.players.FindPrimaryLocal() == nullptr) {
        return MakeError("no primary local player is available");
    }

    InputFrame input = InputFrame::New();
    for (std::size_t i = frames_index + 1; i < parts.size(); ++i) {
        if (!ApplyInputButtonToken(input, parts[i])) {
            return MakeError("unknown input button token");
        }
    }

    state.debug_input_override.active = true;
    state.debug_input_override.player_id = player_id;
    state.debug_input_override.frames_remaining = frames;
    state.debug_input_override.input = input;

    std::ostringstream out;
    out << "{\"ok\":true,\"cmd\":\"input\""
        << ",\"frames\":" << frames
        << ",\"player_id\":";
    if (player_id == kInvalidPlayerId) {
        out << "null";
    } else {
        out << player_id;
    }
    out << "}\n";
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
            << ",\"ent\":";
        if (slot.ent_vid.has_value()) {
            if (const Ent* const ent = state.ents.GetEnt(*slot.ent_vid)) {
                WriteEntJson(out, state, *ent);
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

std::string HandleEntCommand(const State& state, const std::vector<std::string>& parts) {
    if (parts.size() < 2) {
        return MakeError("ent command requires an ent id");
    }
    const int ent_id = ParseIntArg(parts, 1, -1);
    if (ent_id < 0 || static_cast<std::size_t>(ent_id) >= state.ents.ents.size()) {
        return MakeError("ent id is outside the ent array");
    }
    const Ent& ent = state.ents.ents[static_cast<std::size_t>(ent_id)];
    std::ostringstream out;
    out << "{\"ok\":true,\"cmd\":\"ent\",\"ent\":";
    WriteEntJson(out, state, ent);
    out << "}\n";
    return out.str();
}

std::optional<Vec2> GetPrimaryLocalPlayerCenter(const State& state) {
    const PlayerSlot* const player = state.players.FindPrimaryLocal();
    if (player == nullptr || !player->ent_vid.has_value()) {
        return std::nullopt;
    }
    const Ent* const ent = state.ents.GetEnt(*player->ent_vid);
    if (ent == nullptr) {
        return std::nullopt;
    }
    return ent->GetCenter();
}

std::string HandleEntsCommand(const State& state, const std::vector<std::string>& parts) {
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
    limit = std::clamp(limit, 1, static_cast<int>(EntPool::kMaxNumEnts));

    std::optional<Vec2> center;
    if (near_primary_player) {
        center = GetPrimaryLocalPlayerCenter(state);
        if (!center.has_value()) {
            return MakeError("no primary local player ent is available for near query");
        }
    }

    std::ostringstream out;
    int emitted = 0;
    int matching = 0;
    out << "{\"ok\":true,\"cmd\":\"ents\",\"limit\":" << limit << ",\"ents\":[";
    bool first = true;
    for (const Ent& ent : state.ents.ents) {
        if (!ent.active) {
            continue;
        }
        if (center.has_value()) {
            const Vec2 delta = ent.GetCenter() - *center;
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
        WriteEntJson(out, state, ent);
        ++emitted;
    }
    out << "],\"matching\":" << matching << ",\"emitted\":" << emitted << "}\n";
    return out.str();
}

void WriteNetFuzzerConfigJson(std::ostringstream& out, const network::NetFuzzerConfig& config) {
    out << "{\"enabled\":" << (config.enabled ? "true" : "false")
        << ",\"latency_ms\":" << config.latency_ms
        << ",\"jitter_ms\":" << config.jitter_ms
        << ",\"packet_loss_percent\":" << config.packet_loss_percent
        << ",\"duplicate_percent\":" << config.duplicate_percent
        << ",\"reorder_window_packets\":" << config.reorder_window_packets
        << ",\"bandwidth_cap_bytes_per_second\":"
        << config.bandwidth_cap_bytes_per_second
        << ",\"burst_loss_enabled\":"
        << (config.burst_loss_enabled ? "true" : "false")
        << ",\"burst_loss_percent\":" << config.burst_loss_percent
        << ",\"clock_drift_percent\":" << config.clock_drift_percent
        << "}";
}

void WriteNetFuzzerStatsJson(std::ostringstream& out, const network::NetFuzzerStats& stats) {
    out << "{\"packets_sent\":" << stats.packets_sent
        << ",\"packets_received\":" << stats.packets_received
        << ",\"packets_dropped\":" << stats.packets_dropped
        << ",\"packets_duplicated\":" << stats.packets_duplicated
        << ",\"packets_reordered\":" << stats.packets_reordered
        << "}";
}

std::string HandleNetFuzzerCommand(State& state, const std::vector<std::string>& parts) {
    if (parts.size() < 3 || parts[2] == "status") {
        std::ostringstream out;
        out << "{\"ok\":true,\"cmd\":\"net fuzzer\",\"config\":";
        WriteNetFuzzerConfigJson(out, state.net_session.fuzzer_config);
        out << ",\"stats\":";
        WriteNetFuzzerStatsJson(out, state.net_session.fuzzer_stats);
        out << "}\n";
        return out.str();
    }

    if (parts[2] == "off" || parts[2] == "disable") {
        state.net_session.fuzzer_config = network::NetFuzzerConfig{};
        state.net_session.fuzzer_config.enabled = false;
    } else if (parts[2] == "preset") {
        if (parts.size() < 4) {
            return MakeError("net fuzzer preset requires a preset name");
        }
        const std::optional<network::NetFuzzerConfig> preset = NetFuzzerPresetByName(parts[3]);
        if (!preset.has_value()) {
            return MakeError("unknown net fuzzer preset");
        }
        state.net_session.fuzzer_config = *preset;
    } else if (parts[2] == "set") {
        state.net_session.fuzzer_config.enabled = true;
        state.net_session.fuzzer_config.latency_ms =
            std::clamp(ParseFloatArg(parts, 3, state.net_session.fuzzer_config.latency_ms), 0.0F, 300.0F);
        state.net_session.fuzzer_config.jitter_ms =
            std::clamp(ParseFloatArg(parts, 4, state.net_session.fuzzer_config.jitter_ms), 0.0F, 150.0F);
        state.net_session.fuzzer_config.packet_loss_percent =
            std::clamp(ParseFloatArg(parts, 5, state.net_session.fuzzer_config.packet_loss_percent), 0.0F, 25.0F);
        state.net_session.fuzzer_config.duplicate_percent =
            std::clamp(ParseFloatArg(parts, 6, state.net_session.fuzzer_config.duplicate_percent), 0.0F, 25.0F);
        state.net_session.fuzzer_config.reorder_window_packets =
            static_cast<std::uint32_t>(std::clamp(ParseIntArg(
                parts,
                7,
                static_cast<int>(state.net_session.fuzzer_config.reorder_window_packets)
            ), 0, 32));
    } else {
        return MakeError("unknown net fuzzer command");
    }

    std::ostringstream out;
    out << "{\"ok\":true,\"cmd\":\"net fuzzer\",\"config\":";
    WriteNetFuzzerConfigJson(out, state.net_session.fuzzer_config);
    out << ",\"stats\":";
    WriteNetFuzzerStatsJson(out, state.net_session.fuzzer_stats);
    out << "}\n";
    return out.str();
}

std::string HandleNetCommand(State& state, const std::vector<std::string>& parts) {
    if (parts.size() >= 2 && parts[1] == "fuzzer") {
        return HandleNetFuzzerCommand(state, parts);
    }
    if (parts.size() >= 2 && parts[1] == "settings") {
        if (parts.size() >= 3 && parts[2] == "set") {
            if (parts.size() < 5) {
                return MakeError("net settings set requires delay_frames and rollback_frames");
            }
            std::string status;
            const bool ok = network::ScheduleLockstepSettingsChange(
                state,
                static_cast<std::uint32_t>(std::clamp(ParseIntArg(parts, 3, 2), 0, 999)),
                static_cast<std::uint32_t>(std::clamp(ParseIntArg(parts, 4, 12), 0, 999)),
                &status
            );
            if (!ok) {
                return MakeError(status);
            }
            return "{\"ok\":true,\"cmd\":\"net settings set\",\"status\":" +
                JsonString(status) + "}\n";
        }
        if (parts.size() >= 3 && parts[2] == "auto") {
            if (parts.size() < 4) {
                return MakeError("net settings auto requires on or off");
            }
            if (state.net_session.role != network::NetRole::Host) {
                return MakeError("only the host can change auto delay");
            }
            if (parts[3] == "on") {
                state.net_session.lockstep_auto_delay_enabled = true;
            } else if (parts[3] == "off") {
                state.net_session.lockstep_auto_delay_enabled = false;
                state.net_session.lockstep_auto_delay_candidate_age_frames = 0;
            } else {
                return MakeError("net settings auto requires on or off");
            }
        } else if (parts.size() >= 3 && parts[2] != "status") {
            return MakeError("unknown net settings command");
        }
    }

    const std::size_t total_input_records =
        state.net_session.lockstep_input_buffer.RecordCount();
    const std::size_t local_input_records =
        LocalInputRecordCount(state, state.net_session.lockstep_input_buffer);
    const std::size_t remote_input_records =
        total_input_records >= local_input_records ? total_input_records - local_input_records : 0;
    const float simulated_seconds = state.net_session.lockstep_next_frame_to_step == 0
        ? 0.0F
        : static_cast<float>(state.net_session.lockstep_next_frame_to_step) / 60.0F;
    const float rollbacks_per_second = simulated_seconds <= 0.0F
        ? 0.0F
        : static_cast<float>(state.net_session.lockstep_rollback_count) / simulated_seconds;
    const std::uint64_t resolved_predictions =
        state.net_session.lockstep_prediction_miss_count +
        state.net_session.lockstep_prediction_late_match_count;
    const float prediction_miss_rate = resolved_predictions == 0
        ? 0.0F
        : static_cast<float>(state.net_session.lockstep_prediction_miss_count) /
            static_cast<float>(resolved_predictions);

    std::ostringstream out;
    out << "{\"ok\":true,\"cmd\":\"net\""
        << ",\"role\":" << JsonString(NetRoleName(state.net_session.role))
        << ",\"local_player_id\":" << state.net_session.local_player_id
        << ",\"host_player_id\":" << state.net_session.host_player_id
        << ",\"stage_instance_id\":" << state.net_session.stage_instance_id
        << ",\"quest\":" << JsonString(state.net_session.quest_id)
        << ",\"stage\":" << JsonString(state.net_session.quest_stage_id)
        << ",\"seed\":" << state.net_session.stage_seed
        << ",\"input_lockstep_enabled\":" << (state.net_session.input_lockstep_enabled ? "true" : "false")
        << ",\"lockstep_next_frame\":" << state.net_session.lockstep_next_frame_to_step
        << ",\"lockstep_next_local_input_frame\":" << state.net_session.lockstep_next_local_input_frame
        << ",\"lockstep_input_delay_frames\":" << state.net_session.lockstep_input_delay_frames
        << ",\"lockstep_max_rollback_frames\":" << state.net_session.lockstep_max_rollback_frames
        << ",\"lockstep_auto_delay_enabled\":"
        << (state.net_session.lockstep_auto_delay_enabled ? "true" : "false")
        << ",\"lockstep_auto_delay_candidate_frames\":"
        << state.net_session.lockstep_auto_delay_candidate_frames
        << ",\"lockstep_auto_delay_candidate_age_frames\":"
        << state.net_session.lockstep_auto_delay_candidate_age_frames
        << ",\"lockstep_pending_settings\":";
    if (state.net_session.lockstep_pending_settings.has_value()) {
        const network::PendingLockstepSettings& pending =
            *state.net_session.lockstep_pending_settings;
        out << "{\"sequence\":" << pending.sequence
            << ",\"apply_frame\":" << pending.apply_frame
            << ",\"input_delay_frames\":" << pending.input_delay_frames
            << ",\"max_rollback_frames\":" << pending.max_rollback_frames << "}";
    } else {
        out << "null";
    }
    out
        << ",\"lockstep_has_confirmed_hash\":"
        << (state.net_session.lockstep_has_confirmed_hash ? "true" : "false")
        << ",\"lockstep_last_confirmed_hash_frame\":"
        << state.net_session.lockstep_last_confirmed_hash_frame
        << ",\"lockstep_last_confirmed_hash\":"
        << state.net_session.lockstep_last_confirmed_hash
        << ",\"lockstep_hash_send_interval_frames\":"
        << state.net_session.lockstep_hash_send_interval_frames
        << ",\"lockstep_hash_history_count\":"
        << state.net_session.lockstep_hash_history.size()
        << ",\"lockstep_remote_hash_history_count\":"
        << state.net_session.lockstep_remote_hash_history.size()
        << ",\"lockstep_pending_remote_hash_count\":"
        << state.net_session.lockstep_pending_remote_hashes.size()
        << ",\"lockstep_hash_mismatch_count\":"
        << state.net_session.lockstep_hash_mismatch_count
        << ",\"lockstep_last_desync_recovery_mode\":"
        << JsonString(LockstepDesyncRecoveryModeName(
               state.net_session.lockstep_last_desync_recovery_mode
           ))
        << ",\"lockstep_last_mismatch_peer_id\":"
        << state.net_session.lockstep_last_mismatch_peer_id
        << ",\"lockstep_last_mismatch_frame\":"
        << state.net_session.lockstep_last_mismatch_frame
        << ",\"lockstep_last_mismatch_local_hash\":"
        << state.net_session.lockstep_last_mismatch_local_hash
        << ",\"lockstep_last_mismatch_remote_hash\":"
        << state.net_session.lockstep_last_mismatch_remote_hash
        << ",\"lockstep_input_record_count\":" << total_input_records
        << ",\"lockstep_remote_input_record_count\":" << remote_input_records
        << ",\"lockstep_predicted_input_record_count\":"
        << state.net_session.lockstep_input_buffer.PredictedRecordCount()
        << ",\"lockstep_input_wait_block_count\":"
        << state.net_session.lockstep_input_wait_block_count
        << ",\"lockstep_rollback_enabled\":"
        << (state.net_session.lockstep_rollback_enabled ? "true" : "false")
        << ",\"lockstep_rollback_count\":" << state.net_session.lockstep_rollback_count
        << ",\"lockstep_rollbacks_per_second\":" << rollbacks_per_second
        << ",\"lockstep_last_rollback_span\":" << state.net_session.lockstep_last_rollback_span
        << ",\"lockstep_max_rollback_span\":" << state.net_session.lockstep_max_rollback_span
        << ",\"lockstep_last_rollback_replay_ms\":"
        << state.net_session.lockstep_last_rollback_replay_ms
        << ",\"lockstep_avg_rollback_replay_ms\":"
        << (state.net_session.lockstep_rollback_count == 0
                ? 0.0F
                : state.net_session.lockstep_total_rollback_replay_ms /
                    static_cast<float>(state.net_session.lockstep_rollback_count))
        << ",\"lockstep_rollback_snapshots\":"
        << state.net_session.lockstep_rollback_snapshots.size()
        << ",\"lockstep_prediction_miss_count\":"
        << state.net_session.lockstep_prediction_miss_count
        << ",\"lockstep_prediction_late_match_count\":"
        << state.net_session.lockstep_prediction_late_match_count
        << ",\"lockstep_prediction_miss_rate\":" << prediction_miss_rate
        << ",\"lockstep_last_prediction_miss_span\":"
        << state.net_session.lockstep_last_prediction_miss_span
        << ",\"snapshot_resync\":{"
        << "\"pending_request\":"
        << (state.net_session.lockstep_snapshot_resync_pending_request ? "true" : "false")
        << ",\"waiting_for_ack\":"
        << (state.net_session.lockstep_snapshot_resync_waiting_for_ack ? "true" : "false")
        << ",\"target_peer_id\":" << state.net_session.lockstep_snapshot_resync_target_peer_id
        << ",\"active_transfer_id\":"
        << state.net_session.lockstep_snapshot_resync_active_transfer_id
        << ",\"queued_targets\":" << state.net_session.lockstep_snapshot_resync_queue.size()
        << ",\"snapshot_frame\":" << state.net_session.lockstep_snapshot_resync_frame
        << ",\"chunk_count\":" << state.net_session.lockstep_snapshot_resync_chunk_count
        << ",\"total_bytes\":" << state.net_session.lockstep_snapshot_resync_total_bytes
        << ",\"next_chunk_to_send\":"
        << state.net_session.lockstep_snapshot_resync_next_chunk_to_send
        << ",\"received_chunks\":"
        << std::count(
               state.net_session.lockstep_snapshot_resync_received_chunks.begin(),
               state.net_session.lockstep_snapshot_resync_received_chunks.end(),
               static_cast<std::uint8_t>(1)
           )
        << ",\"retry_ticks\":" << state.net_session.lockstep_snapshot_resync_retry_ticks
        << "}"
        << ",\"ent_links\":" << state.net_session.ent_links.size()
        << ",\"fuzzer\":{\"config\":";
    WriteNetFuzzerConfigJson(out, state.net_session.fuzzer_config);
    out << ",\"stats\":";
    WriteNetFuzzerStatsJson(out, state.net_session.fuzzer_stats);
    out << "}"
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
            << ",\"remote_endpoints\":[";
        for (std::size_t i = 0; i < state.net_transport->remotes.size(); ++i) {
            const network::NetRemoteEndpoint& remote = state.net_transport->remotes[i];
            if (i > 0) {
                out << ",";
            }
            out << "{\"endpoint\":" << JsonString(remote.endpoint.address + ":" + std::to_string(remote.endpoint.port))
                << ",\"last_heard_frame\":" << remote.last_heard_frame
                << ",\"players\":[";
            for (std::size_t player_index = 0; player_index < remote.player_ids.size(); ++player_index) {
                if (player_index > 0) {
                    out << ",";
                }
                out << remote.player_ids[player_index];
            }
            out << "]}";
        }
        out << "]}";
    } else {
        out << "null";
    }
    out << "}\n";
    return out.str();
}

std::string HandleFingerprintCommand(const State& state) {
    const CanonicalStateFingerprint canonical = ComputeCanonicalStateFingerprint(state);
    const CanonicalStateFingerprint gameplay = ComputeGameplayDeterminismFingerprint(state);
    const CanonicalStateFingerprint network = ComputeNetworkStateFingerprint(state);
    std::ostringstream out;
    out << "{\"ok\":true,\"cmd\":\"fingerprint\""
        << ",\"canonical\":{\"hash\":" << canonical.value
        << ",\"summary\":" << JsonString(canonical.summary) << "}"
        << ",\"gameplay\":{\"hash\":" << gameplay.value
        << ",\"summary\":" << JsonString(gameplay.summary) << "}"
        << ",\"network\":{\"hash\":" << network.value
        << ",\"summary\":" << JsonString(network.summary) << "}"
        << "}\n";
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

std::string HandleCommand(State& state, std::string_view command) {
    const std::vector<std::string> parts = SplitCommand(command);
    if (parts.empty()) {
        return MakeError("empty command");
    }
    const std::string& op = parts[0];
    if (op == "ping") {
        return "{\"ok\":true,\"cmd\":\"ping\",\"pong\":true}\n";
    }
    if (op == "help") {
        return "{\"ok\":true,\"cmd\":\"help\",\"commands\":[\"ping\",\"status\",\"players\",\"ents [limit]\",\"ents near [radius] [limit]\",\"ent <id>\",\"tiles <x> <y> <w> <h>\",\"net\",\"net settings status\",\"net settings set <delay_frames> <rollback_frames>\",\"net settings auto <on|off>\",\"net fuzzer status\",\"net fuzzer off\",\"net fuzzer preset <name>\",\"net fuzzer set <latency_ms> <jitter_ms> <loss_pct> <duplicate_pct> <reorder_window>\",\"fingerprint\",\"perf\",\"input <frames> [buttons...]\",\"input player <id> <frames> [buttons...]\",\"input clear\",\"input status\"]}\n";
    }
    if (op == "status") {
        return HandleStatusCommand(state);
    }
    if (op == "players") {
        return HandlePlayersCommand(state);
    }
    if (op == "ents") {
        return HandleEntsCommand(state, parts);
    }
    if (op == "ent") {
        return HandleEntCommand(state, parts);
    }
    if (op == "tiles") {
        return HandleTilesCommand(state, parts);
    }
    if (op == "net") {
        return HandleNetCommand(state, parts);
    }
    if (op == "fingerprint") {
        return HandleFingerprintCommand(state);
    }
    if (op == "perf") {
        return HandlePerfCommand(state);
    }
    if (op == "input") {
        return HandleInputCommand(state, parts);
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

void DebugControlServer::Step(State& state) {
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

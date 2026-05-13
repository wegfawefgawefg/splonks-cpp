#include "debug/playback_internal.hpp"

#include "network/net_lobby.hpp"
#include "network/net_session.hpp"
#include "stage_spawning.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace splonks::debug_playback_internal {

namespace {

const char* NetRoleName(network::NetRole role) {
    switch (role) {
    case network::NetRole::Offline:
        return "Offline";
    case network::NetRole::Host:
        return "Host";
    case network::NetRole::Peer:
        return "Peer";
    }
    return "Unknown";
}

constexpr float kNetworkFrameMs = 1000.0F / 60.0F;

std::vector<PlayerId> DebugLocalPlayerIds(const State& state) {
    std::vector<PlayerId> player_ids;
    for (const PlayerSlot& slot : state.players.slots) {
        if (slot.connected && slot.connection_kind == PlayerConnectionKind::Local &&
            slot.player_id != kInvalidPlayerId) {
            player_ids.push_back(slot.player_id);
        }
    }
    return player_ids;
}

std::size_t DebugLocalInputRecordCount(
    const State& state,
    const network::LockstepInputBuffer& buffer
) {
    std::size_t count = 0;
    for (PlayerId player_id : DebugLocalPlayerIds(state)) {
        count += buffer.RecordCountForPlayer(player_id);
    }
    return count;
}

int SuggestedLockstepDelayFrames(float ping_ms, float jitter_ms) {
    constexpr float kSafetyFrames = 1.0F;
    const float one_way_ms = std::max(0.0F, ping_ms) * 0.5F;
    const float jitter_margin_ms = std::max(2.0F, std::max(0.0F, jitter_ms) * 2.0F);
    const float frames = std::ceil((one_way_ms + jitter_margin_ms) / kNetworkFrameMs + kSafetyFrames);
    return static_cast<int>(std::clamp(
        frames,
        static_cast<float>(network::kMinLockstepInputDelayFrames),
        static_cast<float>(network::kMaxLockstepInputDelayFrames)
    ));
}

void SyncLockstepSettingSliderFromActive(
    int& slider_value,
    std::uint32_t& last_synced_active_value,
    std::uint32_t active_value
) {
    const int clamped_active_value = static_cast<int>(active_value);
    const bool has_unscheduled_edit =
        slider_value != static_cast<int>(last_synced_active_value) &&
        slider_value != clamped_active_value;
    if (!has_unscheduled_edit) {
        slider_value = clamped_active_value;
    }
    last_synced_active_value = active_value;
}

const char* ReconnectSpawnModeName(network::NetReconnectSpawnMode mode) {
    switch (mode) {
    case network::NetReconnectSpawnMode::FreshAtEntrance:
        return "Fresh At Entrance";
    case network::NetReconnectSpawnMode::FreshAtHost:
        return "Fresh At Host";
    case network::NetReconnectSpawnMode::RetainedAtEntrance:
        return "Retained At Entrance";
    case network::NetReconnectSpawnMode::RetainedAtLastPosition:
        return "Retained At Last Position";
    case network::NetReconnectSpawnMode::RetainedAtHost:
        return "Retained At Host";
    }
    return "Unknown";
}

const char* MultiplayerRespawnModeName(MultiplayerRespawnMode mode) {
    switch (mode) {
    case MultiplayerRespawnMode::GenerousNextLevel:
        return "Generous Next Level";
    case MultiplayerRespawnMode::NoRespawn:
        return "No Respawn";
    case MultiplayerRespawnMode::RespawnAtEntrance:
        return "Respawn At Entrance";
    }
    return "Unknown";
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

void DrawFuzzerPresetButton(
    const char* label,
    network::NetFuzzerConfig preset,
    network::NetSessionState& session
) {
    if (ImGui::Button(label)) {
        session.fuzzer_config = preset;
    }
}

void DrawFuzzerControls(network::NetFuzzerConfig& config) {
    ImGui::Checkbox("Enabled", &config.enabled);
    ImGui::SliderFloat("Latency ms", &config.latency_ms, 0.0F, 300.0F, "%.1f");
    ImGui::SliderFloat("Jitter ms", &config.jitter_ms, 0.0F, 150.0F, "%.1f");
    ImGui::SliderFloat("Packet loss %", &config.packet_loss_percent, 0.0F, 25.0F, "%.2f");
    ImGui::SliderFloat("Duplicate %", &config.duplicate_percent, 0.0F, 25.0F, "%.2f");

    int reorder_window = static_cast<int>(config.reorder_window_packets);
    ImGui::SliderInt("Reorder window", &reorder_window, 0, 32);
    config.reorder_window_packets = static_cast<std::uint32_t>(std::max(0, reorder_window));

    int bandwidth_cap = static_cast<int>(config.bandwidth_cap_bytes_per_second);
    ImGui::SliderInt("Bandwidth cap B/s", &bandwidth_cap, 0, 256000);
    config.bandwidth_cap_bytes_per_second = static_cast<std::uint32_t>(std::max(0, bandwidth_cap));

    ImGui::Checkbox("Burst loss", &config.burst_loss_enabled);
    ImGui::SliderFloat("Burst loss %", &config.burst_loss_percent, 0.0F, 50.0F, "%.2f");
}

void DrawFuzzerPanel(network::NetSessionState& session) {
    if (!ImGui::CollapsingHeader("Network Fuzzer", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::TextUnformatted("Presets");
    DrawFuzzerPresetButton("Same House", network::NetFuzzerConfig::SameHousePreset(), session);
    ImGui::SameLine();
    DrawFuzzerPresetButton("Same City", network::NetFuzzerConfig::SameCityPreset(), session);
    ImGui::SameLine();
    DrawFuzzerPresetButton("Same State", network::NetFuzzerConfig::SameStatePreset(), session);
    DrawFuzzerPresetButton("TX To CA", network::NetFuzzerConfig::TexasToCaliforniaPreset(), session);
    ImGui::SameLine();
    DrawFuzzerPresetButton("CA To FL", network::NetFuzzerConfig::CaliforniaToFloridaPreset(), session);
    ImGui::SameLine();
    DrawFuzzerPresetButton("TX To Japan", network::NetFuzzerConfig::JapanToTexasPreset(), session);
    DrawFuzzerPresetButton("Bad Wi-Fi", network::NetFuzzerConfig::BadWifiPreset(), session);
    DrawFuzzerControls(session.fuzzer_config);

    ImGui::TextUnformatted("Stats");
    ImGui::Text("Sent: %llu", static_cast<unsigned long long>(session.fuzzer_stats.packets_sent));
    ImGui::SameLine();
    ImGui::Text("Received: %llu", static_cast<unsigned long long>(session.fuzzer_stats.packets_received));
    ImGui::Text("Dropped: %llu", static_cast<unsigned long long>(session.fuzzer_stats.packets_dropped));
    ImGui::SameLine();
    ImGui::Text("Duplicated: %llu", static_cast<unsigned long long>(session.fuzzer_stats.packets_duplicated));
    ImGui::SameLine();
    ImGui::Text("Reordered: %llu", static_cast<unsigned long long>(session.fuzzer_stats.packets_reordered));
}

Vec2 GetDebugBotSpawnPos(const State& state) {
    if (const PlayerSlot* const primary = state.players.FindPrimaryLocal()) {
        if (primary->ent_vid.has_value()) {
            if (const Ent* const ent = state.ents.GetEnt(*primary->ent_vid)) {
                return ent->pos + Vec2::New(12.0F, -2.0F);
            }
        }
    }
    return Vec2::New(24.0F, 24.0F);
}

void AddDebugLocalPlayerBot(State& state, DebugPlayback& debug, const Graphics& graphics) {
    const PlayerId player_id = state.next_debug_local_player_id++;
    (void)state.players.EnsureLocalPlayer(
        player_id,
        "Debug Bot " + std::to_string(player_id),
        false
    );

    const std::optional<VID> bot_vid = SpawnPlayerForPlayerId(state, player_id, GetDebugBotSpawnPos(state));
    if (bot_vid.has_value()) {
        state.UpdateSidForEnt(bot_vid->id, graphics);
    }

    DebugLocalPlayerBot bot;
    bot.player_id = player_id;
    state.debug_local_player_bots.push_back(bot);
    debug.network_status = "Added debug local player " + std::to_string(player_id) + ".";
}

void RemoveDebugLocalPlayerBot(State& state, PlayerId player_id) {
    if (PlayerSlot* const slot = state.players.Find(player_id)) {
        if (slot->ent_vid.has_value()) {
            state.ents.SetInactiveVid(*slot->ent_vid);
        }
    }
    state.players.Remove(player_id);
    state.debug_local_player_bots.erase(
        std::remove_if(
            state.debug_local_player_bots.begin(),
            state.debug_local_player_bots.end(),
            [player_id](const DebugLocalPlayerBot& bot) { return bot.player_id == player_id; }
        ),
        state.debug_local_player_bots.end()
    );
}

void DrawReconnectPolicyControls(State& state) {
    if (!ImGui::CollapsingHeader("Reconnect Policy", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    network::NetSessionState& session = state.net_session;
    if (ImGui::BeginCombo(
            "Spawn mode",
            ReconnectSpawnModeName(session.reconnect_spawn_mode)
        )) {
        constexpr network::NetReconnectSpawnMode kModes[] = {
            network::NetReconnectSpawnMode::FreshAtEntrance,
            network::NetReconnectSpawnMode::FreshAtHost,
            network::NetReconnectSpawnMode::RetainedAtEntrance,
            network::NetReconnectSpawnMode::RetainedAtLastPosition,
            network::NetReconnectSpawnMode::RetainedAtHost,
        };
        for (const network::NetReconnectSpawnMode mode : kModes) {
            const bool selected = session.reconnect_spawn_mode == mode;
            if (ImGui::Selectable(ReconnectSpawnModeName(mode), selected)) {
                session.reconnect_spawn_mode = mode;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    int lifetime_seconds = static_cast<int>(session.retained_player_lifetime_frames / 60U);
    if (ImGui::SliderInt("Retained lifetime sec (0=forever)", &lifetime_seconds, 0, 3600)) {
        constexpr std::uint64_t kFramesPerSecond = 60;
        session.retained_player_lifetime_frames =
            static_cast<std::uint64_t>(std::max(0, lifetime_seconds)) * kFramesPerSecond;
    }

    ImGui::Text("Retained players: %zu", session.retained_players.size());
    ImGui::SameLine();
    if (ImGui::Button("Clear Retained Players")) {
        session.retained_players.clear();
    }

    for (const network::NetRetainedPlayerState& retained : session.retained_players) {
        const std::uint64_t age_frames =
            state.frame > retained.disconnected_frame
                ? state.frame - retained.disconnected_frame
                : 0ULL;
        ImGui::BulletText(
            "player=%u type=%s hp=%u money=%u age=%llus held=%s back=%s",
            retained.player_id,
            EntTypeToString(retained.ent_type),
            retained.health,
            retained.money,
            static_cast<unsigned long long>(age_frames / 60U),
            retained.held_item.valid ? EntTypeToString(retained.held_item.ent_type) : "none",
            retained.back_item.valid ? EntTypeToString(retained.back_item.ent_type) : "none"
        );
    }
}

void DrawRespawnPolicyControls(State& state) {
    if (!ImGui::CollapsingHeader("Respawn Policy", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    const bool transport_open = network::IsTransportOpen(state);
    if (transport_open) {
        ImGui::BeginDisabled();
    }

    if (ImGui::BeginCombo(
            "Mode",
            MultiplayerRespawnModeName(state.multiplayer_respawn_mode)
        )) {
        constexpr MultiplayerRespawnMode kModes[] = {
            MultiplayerRespawnMode::GenerousNextLevel,
            MultiplayerRespawnMode::NoRespawn,
            MultiplayerRespawnMode::RespawnAtEntrance,
        };
        for (const MultiplayerRespawnMode mode : kModes) {
            const bool selected = state.multiplayer_respawn_mode == mode;
            if (ImGui::Selectable(MultiplayerRespawnModeName(mode), selected)) {
                state.multiplayer_respawn_mode = mode;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    if (transport_open) {
        ImGui::EndDisabled();
        ImGui::TextWrapped("Respawn policy is lockstep state. Change it before hosting/joining.");
    }
}

void DrawSnapshotResyncControls(State& state, DebugPlayback& debug) {
    if (!ImGui::CollapsingHeader("Snapshot Resync")) {
        return;
    }

    network::NetSessionState& session = state.net_session;
    ImGui::Text(
        "Pending=%s waiting_ack=%s target=%u transfer=%u chunks=%u bytes=%u retry=%u",
        session.lockstep_snapshot_resync_pending_request ? "yes" : "no",
        session.lockstep_snapshot_resync_waiting_for_ack ? "yes" : "no",
        session.lockstep_snapshot_resync_target_peer_id,
        session.lockstep_snapshot_resync_active_transfer_id,
        session.lockstep_snapshot_resync_chunk_count,
        session.lockstep_snapshot_resync_total_bytes,
        session.lockstep_snapshot_resync_retry_ticks
    );

    if (!network::IsInputLockstepSession(state)) {
        ImGui::TextUnformatted("No active input-lockstep session.");
        return;
    }

    if (session.role == network::NetRole::Host) {
        bool any_remote = false;
        for (const network::NetPeerState& peer : session.peers) {
            if (!peer.connected || peer.player_id == session.local_player_id) {
                continue;
            }
            any_remote = true;
            const std::string label =
                "Force Resync Player " + std::to_string(peer.player_id);
            if (ImGui::Button(label.c_str())) {
                std::string status;
                (void)network::ForceLockstepSnapshotResync(
                    state,
                    peer.player_id,
                    &status
                );
                debug.network_status = status;
            }
        }
        if (!any_remote) {
            ImGui::TextUnformatted("No connected remote peers.");
        }
    } else if (session.role == network::NetRole::Peer) {
        if (ImGui::Button("Request Host Snapshot Resync")) {
            std::string status;
            (void)network::ForceLockstepSnapshotResync(
                state,
                session.local_player_id,
                &status
            );
            debug.network_status = status;
        }
    }
}

void DrawDebugLocalPlayers(State& state, DebugPlayback& debug, const Graphics& graphics) {
    if (!ImGui::CollapsingHeader("Debug Local Players", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::Text("Player slots: %zu", state.players.slots.size());
    ImGui::Text("Debug bots: %zu", state.debug_local_player_bots.size());
    if (ImGui::Button("Add Random Local Player")) {
        AddDebugLocalPlayerBot(state, debug, graphics);
    }
    ImGui::SameLine();
    if (ImGui::Button("Remove All Bots")) {
        std::vector<PlayerId> player_ids;
        player_ids.reserve(state.debug_local_player_bots.size());
        for (const DebugLocalPlayerBot& bot : state.debug_local_player_bots) {
            player_ids.push_back(bot.player_id);
        }
        for (PlayerId player_id : player_ids) {
            RemoveDebugLocalPlayerBot(state, player_id);
        }
        debug.network_status = "Removed all debug local players.";
    }

    for (DebugLocalPlayerBot& bot : state.debug_local_player_bots) {
        ImGui::PushID(static_cast<int>(bot.player_id));
        const PlayerSlot* const slot = state.players.Find(bot.player_id);
        ImGui::Separator();
        const std::string ent_label =
            slot != nullptr && slot->ent_vid.has_value()
                ? std::to_string(slot->ent_vid->id)
                : "none";
        ImGui::Text("Player %u ent=%s", bot.player_id, ent_label.c_str());
        ImGui::Checkbox("Enabled", &bot.enabled);
        ImGui::SameLine();
        ImGui::Checkbox("Jump", &bot.allow_jump);
        ImGui::SameLine();
        ImGui::Checkbox("Tools", &bot.allow_tools);
        ImGui::SameLine();
        if (ImGui::Button("Remove")) {
            const PlayerId player_id = bot.player_id;
            ImGui::PopID();
            RemoveDebugLocalPlayerBot(state, player_id);
            debug.network_status = "Removed debug local player " + std::to_string(player_id) + ".";
            return;
        }
        ImGui::PopID();
    }
}

void DrawHostJoinControls(State& state, DebugPlayback& debug, const Graphics& graphics) {
    if (!ImGui::CollapsingHeader("Host / Join", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    const std::string socket_text = network::IsTransportOpen(state)
        ? "open :" + std::to_string(network::BoundTransportPort(state))
        : "closed";
    ImGui::Text("Socket: %s", socket_text.c_str());
    const bool host_live_lockstep =
        state.net_session.input_lockstep_enabled &&
        state.net_session.role == network::NetRole::Host;
    const bool can_edit_delay = !network::IsTransportOpen(state) || host_live_lockstep;
    if (state.net_session.input_lockstep_enabled) {
        SyncLockstepSettingSliderFromActive(
            debug.network_lockstep_input_delay_frames,
            debug.network_synced_active_input_delay_frames,
            state.net_session.lockstep_input_delay_frames
        );
        SyncLockstepSettingSliderFromActive(
            debug.network_lockstep_rollback_frames,
            debug.network_synced_active_rollback_frames,
            state.net_session.lockstep_max_rollback_frames
        );
    }
    if (!can_edit_delay) {
        ImGui::BeginDisabled();
    }
    ImGui::SliderInt(
        "Input delay frames",
        &debug.network_lockstep_input_delay_frames,
        static_cast<int>(network::kMinLockstepInputDelayFrames),
        static_cast<int>(network::kMaxLockstepInputDelayFrames)
    );
    if (!can_edit_delay) {
        ImGui::EndDisabled();
    }
    debug.network_lockstep_input_delay_frames = static_cast<int>(network::ClampLockstepInputDelayFrames(
        static_cast<std::uint32_t>(std::max(0, debug.network_lockstep_input_delay_frames))
    ));
    ImGui::SliderInt(
        "Rollback frames",
        &debug.network_lockstep_rollback_frames,
        static_cast<int>(network::kMinLockstepMaxRollbackFrames),
        static_cast<int>(network::kMaxLockstepMaxRollbackFrames)
    );
    debug.network_lockstep_rollback_frames = static_cast<int>(network::ClampLockstepMaxRollbackFrames(
        static_cast<std::uint32_t>(std::max(0, debug.network_lockstep_rollback_frames))
    ));
    ImGui::Text(
        "Configured delay: %d frames (%.1fms), rollback: %d frames%s",
        debug.network_lockstep_input_delay_frames,
        static_cast<float>(debug.network_lockstep_input_delay_frames) * kNetworkFrameMs,
        debug.network_lockstep_rollback_frames,
        !network::IsTransportOpen(state)
            ? ""
            : (host_live_lockstep ? " schedule to apply live" : " active next session")
    );
    ImGui::InputInt("Host port", &debug.network_host_port);
    if (ImGui::Button("Host")) {
        const int clamped_port = std::clamp(debug.network_host_port, 1, 65535);
        debug.network_host_port = clamped_port;
        state.net_session.lockstep_max_rollback_frames =
            network::ClampLockstepMaxRollbackFrames(
                static_cast<std::uint32_t>(debug.network_lockstep_rollback_frames)
            );
        (void)network::StartHostSession(
            state,
            static_cast<std::uint16_t>(clamped_port),
            static_cast<std::uint32_t>(debug.network_lockstep_input_delay_frames),
            &debug.network_status
        );
    }
    ImGui::SameLine();
    if (ImGui::Button("Disconnect")) {
        network::DisconnectSession(state, &debug.network_status);
    }

    ImGui::InputText("Join host", debug.network_join_host.data(), debug.network_join_host.size());
    ImGui::InputInt("Join port", &debug.network_join_port);
    if (ImGui::Button("Join")) {
        const int clamped_port = std::clamp(debug.network_join_port, 1, 65535);
        debug.network_join_port = clamped_port;
        (void)network::JoinHostSession(
            state,
            std::string(debug.network_join_host.data()),
            static_cast<std::uint16_t>(clamped_port),
            &debug.network_status
        );
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload Synced Stage")) {
        (void)network::ReloadSyncedQuestStage(state, graphics, &debug.network_status);
    }

    if (state.net_session.input_lockstep_enabled) {
        ImGui::SeparatorText("Live Lockstep Settings");
        ImGui::Text(
            "Active delay=%u rollback=%u",
            state.net_session.lockstep_input_delay_frames,
            state.net_session.lockstep_max_rollback_frames
        );
        if (state.net_session.lockstep_pending_settings.has_value()) {
            const network::PendingLockstepSettings& pending =
                *state.net_session.lockstep_pending_settings;
            ImGui::Text(
                "Pending seq=%u apply=%llu delay=%u rollback=%u",
                pending.sequence,
                static_cast<unsigned long long>(pending.apply_frame),
                pending.input_delay_frames,
                pending.max_rollback_frames
            );
        } else {
            ImGui::TextUnformatted("Pending: none");
        }

        const bool host_active = state.net_session.role == network::NetRole::Host;
        if (!host_active) {
            ImGui::BeginDisabled();
        }
        ImGui::Checkbox("Auto delay", &state.net_session.lockstep_auto_delay_enabled);
        ImGui::SameLine();
        ImGui::Text(
            "candidate=%u age=%u",
            state.net_session.lockstep_auto_delay_candidate_frames,
            state.net_session.lockstep_auto_delay_candidate_age_frames
        );
        if (ImGui::Button("Schedule Delay / Rollback")) {
            (void)network::ScheduleLockstepSettingsChange(
                state,
                static_cast<std::uint32_t>(debug.network_lockstep_input_delay_frames),
                static_cast<std::uint32_t>(debug.network_lockstep_rollback_frames),
                &debug.network_status
            );
        }
        if (!host_active) {
            ImGui::EndDisabled();
        }
    }

    if (state.net_transport && !state.net_transport->last_error.empty()) {
        ImGui::TextWrapped("Transport error: %s", state.net_transport->last_error.c_str());
    }
    if (!state.net_session.peers.empty()) {
        ImGui::Text("Peers:");
        for (const network::NetPeerState& peer : state.net_session.peers) {
            const int suggested_delay = SuggestedLockstepDelayFrames(peer.estimated_ping_ms, peer.jitter_ms);
            ImGui::BulletText(
                "player=%u %s %s:%u ping=%.1fms jitter=%.1fms suggested_delay=%d",
                peer.player_id,
                peer.display_name.c_str(),
                peer.endpoint_address.c_str(),
                peer.endpoint_port,
                peer.estimated_ping_ms,
                peer.jitter_ms,
                suggested_delay
            );
        }
    }
}

} // namespace

void DrawNetworkWindow(DebugPlayback& debug, State& state, const Graphics& graphics) {
    if (!debug.network_window_visible) {
        return;
    }

    ImGui::SetNextWindowBgAlpha(0.9F);
    ImGui::SetNextWindowPos(ImVec2(760.0F, 120.0F), ImGuiCond_FirstUseEver);
    const ImVec2 display_size = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowSizeConstraints(
        ImVec2(360.0F, 180.0F),
        ImVec2(display_size.x, std::max(220.0F, display_size.y - 24.0F))
    );
    if (!ImGui::Begin("Debug: Network", &debug.network_window_visible)) {
        ImGui::End();
        return;
    }

    network::NetSessionState& session = state.net_session;
    ImGui::Text("Role: %s", NetRoleName(session.role));
    ImGui::Text("Local player: %u", session.local_player_id);
    ImGui::Text("Host player: %u", session.host_player_id);
    ImGui::Text("Stage instance: %llu", static_cast<unsigned long long>(session.stage_instance_id));
    ImGui::Text(
        "Synced stage: %s/%s seed=%u",
        session.quest_id.c_str(),
        session.quest_stage_id.c_str(),
        session.stage_seed
    );
    if (session.input_lockstep_enabled) {
        ImGui::TextUnformatted("Input lockstep: enabled");
        const std::size_t total_input_records = session.lockstep_input_buffer.RecordCount();
        const std::size_t local_input_records =
            DebugLocalInputRecordCount(state, session.lockstep_input_buffer);
        const std::size_t remote_input_records =
            total_input_records >= local_input_records ? total_input_records - local_input_records : 0;
        const float simulated_seconds = session.lockstep_next_frame_to_step == 0
            ? 0.0F
            : static_cast<float>(session.lockstep_next_frame_to_step) / 60.0F;
        const float rollbacks_per_second = simulated_seconds <= 0.0F
            ? 0.0F
            : static_cast<float>(session.lockstep_rollback_count) / simulated_seconds;
        const std::uint64_t resolved_predictions =
            session.lockstep_prediction_miss_count + session.lockstep_prediction_late_match_count;
        const float prediction_miss_rate = resolved_predictions == 0
            ? 0.0F
            : static_cast<float>(session.lockstep_prediction_miss_count) /
                static_cast<float>(resolved_predictions);
        ImGui::Text(
            "Frame: next=%llu local_inputs=%llu delay=%u (%.1fms)",
            static_cast<unsigned long long>(session.lockstep_next_frame_to_step),
            static_cast<unsigned long long>(session.lockstep_next_local_input_frame),
            session.lockstep_input_delay_frames,
            static_cast<float>(session.lockstep_input_delay_frames) * kNetworkFrameMs
        );
        if (session.lockstep_has_confirmed_hash) {
            ImGui::Text(
                "Confirmed: frame=%llu hash=%llu",
                static_cast<unsigned long long>(session.lockstep_last_confirmed_hash_frame),
                static_cast<unsigned long long>(session.lockstep_last_confirmed_hash)
            );
        } else {
            ImGui::Text("Confirmed: none");
        }
        int hash_interval = static_cast<int>(session.lockstep_hash_send_interval_frames);
        if (session.role == network::NetRole::Host &&
            ImGui::SliderInt("Hash send interval", &hash_interval, 1, 120)) {
            session.lockstep_hash_send_interval_frames =
                static_cast<std::uint32_t>(std::max(1, hash_interval));
        } else if (session.role != network::NetRole::Host) {
            ImGui::Text("Hash send interval: %u", session.lockstep_hash_send_interval_frames);
        }
        ImGui::Text(
            "Input buffer: total=%zu remote=%zu predicted=%zu wait-blocks=%llu",
            total_input_records,
            remote_input_records,
            session.lockstep_input_buffer.PredictedRecordCount(),
            static_cast<unsigned long long>(session.lockstep_input_wait_block_count)
        );
        ImGui::Text(
            "Rollback: %s count=%llu %.3f/s last=%u max=%u replay=%.3fms avg=%.3fms snapshots=%zu",
            session.lockstep_rollback_enabled ? "enabled" : "disabled",
            static_cast<unsigned long long>(session.lockstep_rollback_count),
            rollbacks_per_second,
            session.lockstep_last_rollback_span,
            session.lockstep_max_rollback_span,
            session.lockstep_last_rollback_replay_ms,
            session.lockstep_rollback_count == 0
                ? 0.0F
                : session.lockstep_total_rollback_replay_ms /
                    static_cast<float>(session.lockstep_rollback_count),
            session.lockstep_rollback_snapshots.size()
        );
        ImGui::Text(
            "Prediction: misses=%llu late-matches=%llu miss-rate=%.1f%% last-miss-span=%u",
            static_cast<unsigned long long>(session.lockstep_prediction_miss_count),
            static_cast<unsigned long long>(session.lockstep_prediction_late_match_count),
            prediction_miss_rate * 100.0F,
            session.lockstep_last_prediction_miss_span
        );
        ImGui::Text(
            "Hash history=%zu remote=%zu pending=%zu mismatches=%llu recovery=%s",
            session.lockstep_hash_history.size(),
            session.lockstep_remote_hash_history.size(),
            session.lockstep_pending_remote_hashes.size(),
            static_cast<unsigned long long>(session.lockstep_hash_mismatch_count),
            LockstepDesyncRecoveryModeName(session.lockstep_last_desync_recovery_mode)
        );
        if (session.lockstep_hash_mismatch_count > 0) {
            ImGui::Text(
                "Last mismatch: peer=%u frame=%llu local=%llu remote=%llu",
                session.lockstep_last_mismatch_peer_id,
                static_cast<unsigned long long>(session.lockstep_last_mismatch_frame),
                static_cast<unsigned long long>(session.lockstep_last_mismatch_local_hash),
                static_cast<unsigned long long>(session.lockstep_last_mismatch_remote_hash)
            );
        }
    }
    ImGui::Separator();

    DrawFuzzerPanel(session);
    ImGui::Separator();

    ImGui::Text("Ent links: %zu", session.ent_links.size());
    ImGui::Separator();

    DrawHostJoinControls(state, debug, graphics);
    ImGui::Separator();

    DrawRespawnPolicyControls(state);
    ImGui::Separator();

    DrawReconnectPolicyControls(state);
    ImGui::Separator();

    DrawSnapshotResyncControls(state, debug);
    ImGui::Separator();

    DrawDebugLocalPlayers(state, debug, graphics);
    ImGui::End();
}

} // namespace splonks::debug_playback_internal

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
    ImGui::InputInt("Host port", &debug.network_host_port);
    if (ImGui::Button("Host")) {
        const int clamped_port = std::clamp(debug.network_host_port, 1, 65535);
        debug.network_host_port = clamped_port;
        (void)network::StartHostSession(
            state,
            static_cast<std::uint16_t>(clamped_port),
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

    if (state.net_transport && !state.net_transport->last_error.empty()) {
        ImGui::TextWrapped("Transport error: %s", state.net_transport->last_error.c_str());
    }
    if (!state.net_session.peers.empty()) {
        ImGui::Text("Peers:");
        for (const network::NetPeerState& peer : state.net_session.peers) {
            ImGui::BulletText(
                "player=%u %s %s:%u",
                peer.player_id,
                peer.display_name.c_str(),
                peer.endpoint_address.c_str(),
                peer.endpoint_port
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
        ImGui::Text(
            "Frame: next=%llu local_inputs=%llu delay=%u",
            static_cast<unsigned long long>(session.lockstep_next_frame_to_step),
            static_cast<unsigned long long>(session.lockstep_next_local_input_frame),
            session.lockstep_input_delay_frames
        );
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

    DrawDebugLocalPlayers(state, debug, graphics);
    ImGui::End();
}

} // namespace splonks::debug_playback_internal

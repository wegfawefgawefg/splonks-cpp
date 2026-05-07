#include "debug/playback_internal.hpp"

#include "network/net_event_apply.hpp"
#include "network/net_lobby.hpp"
#include "network/net_session.hpp"
#include "stage_spawning.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace splonks::debug_playback_internal {

namespace {

const char* NetRoleName(network::NetRole role) {
    switch (role) {
    case network::NetRole::Offline:
        return "Offline";
    case network::NetRole::Coordinator:
        return "Coordinator";
    case network::NetRole::Peer:
        return "Peer";
    }
    return "Unknown";
}

const char* NetEventTypeName(network::NetEventType type) {
    switch (type) {
    case network::NetEventType::None:
        return "None";
    case network::NetEventType::PeerJoined:
        return "PeerJoined";
    case network::NetEventType::PeerLeft:
        return "PeerLeft";
    case network::NetEventType::PlayerSpawned:
        return "PlayerSpawned";
    case network::NetEventType::PlayerDespawned:
        return "PlayerDespawned";
    case network::NetEventType::StageLoaded:
        return "StageLoaded";
    case network::NetEventType::StageTransitionStarted:
        return "StageTransitionStarted";
    case network::NetEventType::StageTransitionCommitted:
        return "StageTransitionCommitted";
    case network::NetEventType::RepairSnapshot:
        return "RepairSnapshot";
    case network::NetEventType::ActionRequest:
        return "ActionRequest";
    case network::NetEventType::EntitySpawned:
        return "EntitySpawned";
    case network::NetEventType::EntityDeactivated:
        return "EntityDeactivated";
    case network::NetEventType::EntityStatePatched:
        return "EntityStatePatched";
    case network::NetEventType::EntityHeld:
        return "EntityHeld";
    case network::NetEventType::EntityDropped:
        return "EntityDropped";
    case network::NetEventType::EntityThrown:
        return "EntityThrown";
    case network::NetEventType::EntityDamaged:
        return "EntityDamaged";
    case network::NetEventType::TileChanged:
        return "TileChanged";
    case network::NetEventType::FluidCellPatched:
        return "FluidCellPatched";
    case network::NetEventType::TileBroken:
        return "TileBroken";
    case network::NetEventType::RopeTilePlaced:
        return "RopeTilePlaced";
    case network::NetEventType::PlayerStatePatched:
        return "PlayerStatePatched";
    case network::NetEventType::RunStatePatched:
        return "RunStatePatched";
    case network::NetEventType::PresentationCommand:
        return "PresentationCommand";
    }
    return "Unknown";
}

const char* NetEventLogPhaseName(network::NetEventLogPhase phase) {
    switch (phase) {
    case network::NetEventLogPhase::EnqueuedOutbound:
        return "outbound";
    case network::NetEventLogPhase::EnqueuedOrdered:
        return "ordered";
    case network::NetEventLogPhase::Applied:
        return "applied";
    case network::NetEventLogPhase::SkippedLocalApply:
        return "skip-local";
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
    ImGui::SliderFloat("Clock drift %", &config.clock_drift_percent, -5.0F, 5.0F, "%.3f");
}

void DrawRecentOrderedEvents(const network::NetSessionState& session) {
    if (!ImGui::CollapsingHeader("Ordered Events")) {
        return;
    }

    constexpr std::size_t kMaxRows = 12;
    const std::size_t total = session.ordered_events.size();
    const std::size_t begin = total > kMaxRows ? total - kMaxRows : 0;
    for (std::size_t i = begin; i < total; ++i) {
        const network::NetEvent& event = session.ordered_events[i];
        ImGui::Text(
            "#%llu order=%llu player=%u type=%s applied=%s",
            static_cast<unsigned long long>(event.header.event_id),
            static_cast<unsigned long long>(event.header.coordinator_order),
            event.header.source_player_id,
            NetEventTypeName(event.type),
            session.HasAppliedEvent(event.header.event_id) ? "yes" : "no"
        );
    }
}

void DrawEventLog(network::NetSessionState& session) {
    if (!ImGui::CollapsingHeader("Event Log")) {
        return;
    }
    ImGui::Text("Entries: %zu", session.event_log.size());
    ImGui::SameLine();
    if (ImGui::Button("Clear Event Log")) {
        session.event_log.clear();
    }

    constexpr std::size_t kMaxRows = 64;
    const std::size_t total = session.event_log.size();
    const std::size_t begin = total > kMaxRows ? total - kMaxRows : 0;
    for (std::size_t i = begin; i < total; ++i) {
        const network::NetEventLogEntry& entry = session.event_log[i];
        ImGui::Text(
            "%s #%llu order=%llu player=%u frame=%llu type=%s",
            NetEventLogPhaseName(entry.phase),
            static_cast<unsigned long long>(entry.event_id),
            static_cast<unsigned long long>(entry.coordinator_order),
            entry.source_player_id,
            static_cast<unsigned long long>(entry.source_local_frame),
            NetEventTypeName(entry.type)
        );
    }
}

Vec2 GetDebugBotSpawnPos(const State& state) {
    if (const PlayerSlot* const primary = state.players.FindPrimaryLocal()) {
        if (primary->entity_vid.has_value()) {
            if (const Entity* const entity = state.entity_manager.GetEntity(*primary->entity_vid)) {
                return entity->pos + Vec2::New(12.0F, -2.0F);
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
        state.UpdateSidForEntity(bot_vid->id, graphics);
    }

    DebugLocalPlayerBot bot;
    bot.player_id = player_id;
    state.debug_local_player_bots.push_back(bot);
    debug.network_status = "Added debug local player " + std::to_string(player_id) + ".";
}

void RemoveDebugLocalPlayerBot(State& state, PlayerId player_id) {
    if (PlayerSlot* const slot = state.players.Find(player_id)) {
        if (slot->entity_vid.has_value()) {
            state.entity_manager.SetInactiveVid(*slot->entity_vid);
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
        const std::string entity_label =
            slot != nullptr && slot->entity_vid.has_value()
                ? std::to_string(slot->entity_vid->id)
                : "none";
        ImGui::Text("Player %u entity=%s", bot.player_id, entity_label.c_str());
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

void DrawMovementReplicationControls(State& state) {
    if (!ImGui::CollapsingHeader("Movement Replication", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    if (!state.net_transport) {
        ImGui::TextUnformatted("Transport not initialized.");
        return;
    }

    network::NetTransportRuntime& transport = *state.net_transport;
    int snapshot_interval = static_cast<int>(transport.snapshot_send_interval_frames);
    ImGui::SliderInt("Snapshot interval frames", &snapshot_interval, 1, 20);
    transport.snapshot_send_interval_frames = static_cast<std::uint32_t>(
        std::clamp(snapshot_interval, 1, 20)
    );
    ImGui::SliderFloat("Remote interp strength", &transport.remote_interpolation_strength, 0.01F, 1.0F, "%.3f");
    int interp_delay = static_cast<int>(transport.remote_interpolation_delay_frames);
    ImGui::SliderInt("Remote interp delay frames", &interp_delay, 0, 20);
    transport.remote_interpolation_delay_frames = static_cast<std::uint32_t>(
        std::clamp(interp_delay, 0, 20)
    );
    ImGui::SliderFloat("Remote snap distance", &transport.remote_snap_distance, 1.0F, 128.0F, "%.1f");
    ImGui::Text("Remote targets: %zu", transport.remote_player_targets.size());
    for (const network::NetRemotePlayerTarget& target : transport.remote_player_targets) {
        ImGui::BulletText(
            "player=%u seq=%u pos=(%.1f, %.1f) vel=(%.2f, %.2f) age=%llu",
            target.player_id,
            target.sequence,
            target.pos_x,
            target.pos_y,
            target.vel_x,
            target.vel_y,
            static_cast<unsigned long long>(state.frame - target.last_received_frame)
        );
    }
}

void DrawDurableReplicationStatus(const State& state) {
    if (!ImGui::CollapsingHeader("Durable Replication", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    const network::NetSessionState& session = state.net_session;
    ImGui::Text(
        "Expected/apply order: %llu / %llu",
        static_cast<unsigned long long>(session.next_expected_coordinator_order),
        static_cast<unsigned long long>(session.highest_applied_coordinator_order)
    );
    ImGui::Text("Entity id aliases: %zu", session.entity_id_aliases.size());
    if (!state.net_transport) {
        ImGui::TextUnformatted("Transport not initialized.");
        return;
    }

    const network::NetTransportRuntime& transport = *state.net_transport;
    ImGui::Text("Remote endpoints: %zu", transport.remotes.size());
    for (const network::NetRemoteEndpoint& remote : transport.remotes) {
        ImGui::BulletText(
            "%s acked_order=%llu players=%zu last_heard_age=%llu",
            network::EndpointToString(remote.endpoint).c_str(),
            static_cast<unsigned long long>(remote.highest_acked_coordinator_order),
            remote.player_ids.size(),
            static_cast<unsigned long long>(state.frame - remote.last_heard_frame)
        );
    }
}

} // namespace

void DrawNetworkWindow(DebugPlayback& debug, State& state, const Graphics& graphics) {
    if (!debug.network_window_visible) {
        return;
    }

    ImGui::SetNextWindowBgAlpha(0.9F);
    ImGui::SetNextWindowPos(ImVec2(760.0F, 120.0F), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Debug: Network", &debug.network_window_visible)) {
        ImGui::End();
        return;
    }

    network::NetSessionState& session = state.net_session;
    ImGui::Text("Role: %s", NetRoleName(session.role));
    ImGui::Text("Local player: %u", session.local_player_id);
    ImGui::Text("Coordinator player: %u", session.coordinator_player_id);
    ImGui::Text("Stage instance: %llu", static_cast<unsigned long long>(session.stage_instance_id));
    ImGui::Text(
        "Synced stage: %s/%s seed=%u",
        session.quest_id.c_str(),
        session.quest_stage_id.c_str(),
        session.stage_seed
    );
    ImGui::Text("Next local event: %llu", static_cast<unsigned long long>(session.next_local_event_id));
    ImGui::Text("Next coordinator order: %llu", static_cast<unsigned long long>(session.next_coordinator_order));
    ImGui::Separator();

    ImGui::Text("Pending outbound events: %zu", session.pending_outbound_events.size());
    ImGui::Text("Ordered events: %zu", session.ordered_events.size());
    ImGui::Text("Applied events: %zu", session.applied_event_ids.size());
    ImGui::Text("Entity links: %zu", session.entity_links.size());
    ImGui::Separator();

    DrawHostJoinControls(state, debug, graphics);
    ImGui::Separator();

    DrawMovementReplicationControls(state);
    ImGui::Separator();

    DrawDurableReplicationStatus(state);
    ImGui::Separator();

    DrawDebugLocalPlayers(state, debug, graphics);
    ImGui::Separator();

    ImGui::TextUnformatted("Net Event Harness");
    if (ImGui::Button("Apply Ordered Events")) {
        const std::size_t count = network::ApplyOrderedEvents(session, state);
        debug.network_status = "Applied " + std::to_string(count) + " ordered events.";
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Queues")) {
        session.pending_outbound_events.clear();
        session.ordered_events.clear();
        session.applied_event_ids.clear();
        debug.network_status = "Cleared network event queues.";
    }
    if (!debug.network_status.empty()) {
        ImGui::TextWrapped("%s", debug.network_status.c_str());
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Fuzzer Presets");
    DrawFuzzerPresetButton("LAN", network::NetFuzzerConfig::LanPreset(), session);
    ImGui::SameLine();
    DrawFuzzerPresetButton("Same Region", network::NetFuzzerConfig::SameRegionPreset(), session);
    ImGui::SameLine();
    DrawFuzzerPresetButton("US Cross Country", network::NetFuzzerConfig::UsCrossCountryPreset(), session);
    DrawFuzzerPresetButton("Japan To Texas", network::NetFuzzerConfig::JapanToTexasPreset(), session);
    ImGui::SameLine();
    DrawFuzzerPresetButton("Bad Wi-Fi", network::NetFuzzerConfig::BadWifiPreset(), session);
    DrawFuzzerControls(session.fuzzer_config);

    ImGui::Separator();
    ImGui::TextUnformatted("Fuzzer Stats");
    ImGui::Text("Sent: %llu", static_cast<unsigned long long>(session.fuzzer_stats.packets_sent));
    ImGui::Text("Received: %llu", static_cast<unsigned long long>(session.fuzzer_stats.packets_received));
    ImGui::Text("Dropped: %llu", static_cast<unsigned long long>(session.fuzzer_stats.packets_dropped));
    ImGui::Text("Duplicated: %llu", static_cast<unsigned long long>(session.fuzzer_stats.packets_duplicated));
    ImGui::Text("Reordered: %llu", static_cast<unsigned long long>(session.fuzzer_stats.packets_reordered));

    DrawRecentOrderedEvents(session);
    DrawEventLog(session);
    ImGui::End();
}

} // namespace splonks::debug_playback_internal

#include "debug/playback_internal.hpp"

#include "network/net_message_apply.hpp"
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
    case network::NetRole::Coordinator:
        return "Coordinator";
    case network::NetRole::Peer:
        return "Peer";
    }
    return "Unknown";
}

const char* NetMessageTypeName(network::NetMessageType type) {
    switch (type) {
    case network::NetMessageType::None:
        return "None";
    case network::NetMessageType::PeerJoined:
        return "PeerJoined";
    case network::NetMessageType::PeerLeft:
        return "PeerLeft";
    case network::NetMessageType::PlayerSpawned:
        return "PlayerSpawned";
    case network::NetMessageType::PlayerDespawned:
        return "PlayerDespawned";
    case network::NetMessageType::StageLoaded:
        return "StageLoaded";
    case network::NetMessageType::StageTransitionStarted:
        return "StageTransitionStarted";
    case network::NetMessageType::StageTransitionCommitted:
        return "StageTransitionCommitted";
    case network::NetMessageType::RepairSnapshot:
        return "RepairSnapshot";
    case network::NetMessageType::ActionRequest:
        return "ActionRequest";
    case network::NetMessageType::EntitySpawned:
        return "EntitySpawned";
    case network::NetMessageType::EntityDeactivated:
        return "EntityDeactivated";
    case network::NetMessageType::EntityStatePatched:
        return "EntityStatePatched";
    case network::NetMessageType::EntityHeld:
        return "EntityHeld";
    case network::NetMessageType::EntityDropped:
        return "EntityDropped";
    case network::NetMessageType::EntityThrown:
        return "EntityThrown";
    case network::NetMessageType::EntityDamaged:
        return "EntityDamaged";
    case network::NetMessageType::TileChanged:
        return "TileChanged";
    case network::NetMessageType::FluidCellPatched:
        return "FluidCellPatched";
    case network::NetMessageType::StageLightAdded:
        return "StageLightAdded";
    case network::NetMessageType::StageLightRemoved:
        return "StageLightRemoved";
    case network::NetMessageType::TileBroken:
        return "TileBroken";
    case network::NetMessageType::PlayerStatePatched:
        return "PlayerStatePatched";
    case network::NetMessageType::RunStatePatched:
        return "RunStatePatched";
    case network::NetMessageType::PresentationCommand:
        return "PresentationCommand";
    }
    return "Unknown";
}

const char* NetMessageLogPhaseName(network::NetMessageLogPhase phase) {
    switch (phase) {
    case network::NetMessageLogPhase::EnqueuedOutbound:
        return "outbound";
    case network::NetMessageLogPhase::EnqueuedOrdered:
        return "ordered";
    case network::NetMessageLogPhase::Applied:
        return "applied";
    case network::NetMessageLogPhase::SkippedLocalApply:
        return "skip-local";
    }
    return "unknown";
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

const char* PlayerConnectionKindName(PlayerConnectionKind kind) {
    switch (kind) {
    case PlayerConnectionKind::Local:
        return "local";
    case PlayerConnectionKind::Remote:
        return "remote";
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

const Entity* FindPlayerEntityForTarget(
    const State& state,
    const network::NetRemotePlayerTarget& target,
    const PlayerSlot** slot_out
) {
    const PlayerSlot* const slot = state.players.Find(target.player_id);
    if (slot_out != nullptr) {
        *slot_out = slot;
    }
    if (slot == nullptr || !slot->entity_vid.has_value()) {
        return nullptr;
    }
    return state.entity_manager.GetEntity(*slot->entity_vid);
}

float Vec2Distance(float ax, float ay, float bx, float by) {
    const float dx = ax - bx;
    const float dy = ay - by;
    return std::sqrt(dx * dx + dy * dy);
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

void DrawRecentOrderedMessages(const network::NetSessionState& session) {
    if (!ImGui::CollapsingHeader("Ordered Messages")) {
        return;
    }

    constexpr std::size_t kMaxRows = 12;
    const std::size_t total = session.ordered_messages.size();
    const std::size_t begin = total > kMaxRows ? total - kMaxRows : 0;
    for (std::size_t i = begin; i < total; ++i) {
        const network::NetMessage& message = session.ordered_messages[i];
        ImGui::Text(
            "#%llu order=%llu player=%u type=%s applied=%s",
            static_cast<unsigned long long>(message.header.message_id),
            static_cast<unsigned long long>(message.header.coordinator_order),
            message.header.source_player_id,
            NetMessageTypeName(message.type),
            session.HasAppliedMessage(message.header.message_id) ? "yes" : "no"
        );
    }
}

void DrawMessageLog(network::NetSessionState& session) {
    if (!ImGui::CollapsingHeader("Message Log")) {
        return;
    }
    ImGui::Text("Entries: %zu", session.message_log.size());
    ImGui::SameLine();
    if (ImGui::Button("Clear Message Log")) {
        session.message_log.clear();
    }

    constexpr std::size_t kMaxRows = 64;
    const std::size_t total = session.message_log.size();
    const std::size_t begin = total > kMaxRows ? total - kMaxRows : 0;
    for (std::size_t i = begin; i < total; ++i) {
        const network::NetMessageLogEntry& entry = session.message_log[i];
        ImGui::Text(
            "%s #%llu order=%llu player=%u frame=%llu type=%s",
            NetMessageLogPhaseName(entry.phase),
            static_cast<unsigned long long>(entry.message_id),
            static_cast<unsigned long long>(entry.coordinator_order),
            entry.source_player_id,
            static_cast<unsigned long long>(entry.source_local_frame),
            NetMessageTypeName(entry.type)
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
            EntityTypeToString(retained.entity_type),
            retained.health,
            retained.money,
            static_cast<unsigned long long>(age_frames / 60U),
            retained.held_item.valid ? EntityTypeToString(retained.held_item.entity_type) : "none",
            retained.back_item.valid ? EntityTypeToString(retained.back_item.entity_type) : "none"
        );
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
        const PlayerSlot* slot = nullptr;
        const Entity* const entity = FindPlayerEntityForTarget(state, target, &slot);
        const std::uint64_t age_frames =
            state.frame > target.last_received_frame ? state.frame - target.last_received_frame : 0ULL;
        ImGui::BulletText(
            "player=%u %s seq=%u pos=(%.1f, %.1f) vel=(%.2f, %.2f) age=%llu",
            target.player_id,
            slot != nullptr ? PlayerConnectionKindName(slot->connection_kind) : "missing",
            target.sequence,
            target.pos_x,
            target.pos_y,
            target.vel_x,
            target.vel_y,
            static_cast<unsigned long long>(age_frames)
        );
        ImGui::Indent();
        if (entity == nullptr) {
            ImGui::TextDisabled("no local entity for player target");
        } else {
            const float pos_dist =
                Vec2Distance(entity->pos.x, entity->pos.y, target.pos_x, target.pos_y);
            const float vel_dist =
                Vec2Distance(entity->vel.x, entity->vel.y, target.vel_x, target.vel_y);
            const bool over_snap = pos_dist > transport.remote_snap_distance;
            ImGui::Text(
                "local pos=(%.2f, %.2f) target=(%.2f, %.2f) d=(%.2f, %.2f) |d|=%.2f %s",
                entity->pos.x,
                entity->pos.y,
                target.pos_x,
                target.pos_y,
                target.pos_x - entity->pos.x,
                target.pos_y - entity->pos.y,
                pos_dist,
                over_snap ? "SNAP" : "smooth"
            );
            ImGui::Text(
                "local vel=(%.2f, %.2f) target=(%.2f, %.2f) d=(%.2f, %.2f) |d|=%.2f",
                entity->vel.x,
                entity->vel.y,
                target.vel_x,
                target.vel_y,
                target.vel_x - entity->vel.x,
                target.vel_y - entity->vel.y,
                vel_dist
            );
            ImGui::Text(
                "fall=%u/%u coyote=%u/%u stun=%u/%u grounded=%s/%s condition=%s/%s hp=%u/%u",
                entity->fall_timer,
                target.fall_timer,
                entity->coyote_time,
                target.coyote_time,
                entity->stun_timer,
                target.stun_timer,
                entity->grounded ? "yes" : "no",
                target.grounded != 0 ? "yes" : "no",
                EntityConditionName(entity->condition),
                EntityConditionName(static_cast<EntityCondition>(target.condition)),
                entity->health,
                target.health
            );
            ImGui::Text(
                "anim=%u/%u frame=%u/%u held=%s by=%s",
                entity->frame_data_animator.animation_id,
                target.animation_id,
                static_cast<unsigned int>(entity->frame_data_animator.current_frame),
                target.animation_frame,
                entity->holding_vid.has_value() ? "yes" : "no",
                entity->held_by_vid.has_value() ? "yes" : "no"
            );
        }
        ImGui::Unindent();
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
    ImGui::Text("Next local message: %llu", static_cast<unsigned long long>(session.next_local_message_id));
    ImGui::Text("Next coordinator order: %llu", static_cast<unsigned long long>(session.next_coordinator_order));
    ImGui::Separator();

    ImGui::Text("Pending outbound messages: %zu", session.pending_outbound_messages.size());
    ImGui::Text("Ordered messages: %zu", session.ordered_messages.size());
    ImGui::Text("Applied messages: %zu", session.applied_message_ids.size());
    ImGui::Text("Entity links: %zu", session.entity_links.size());
    ImGui::Separator();

    DrawHostJoinControls(state, debug, graphics);
    ImGui::Separator();

    DrawReconnectPolicyControls(state);
    ImGui::Separator();

    DrawMovementReplicationControls(state);
    ImGui::Separator();

    DrawDurableReplicationStatus(state);
    ImGui::Separator();

    DrawDebugLocalPlayers(state, debug, graphics);
    ImGui::Separator();

    ImGui::TextUnformatted("Net Message Harness");
    if (ImGui::Button("Apply Ordered Messages")) {
        const std::size_t count = network::ApplyOrderedMessages(session, state);
        debug.network_status = "Applied " + std::to_string(count) + " ordered messages.";
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Queues")) {
        session.pending_outbound_messages.clear();
        session.ordered_messages.clear();
        session.applied_message_ids.clear();
        debug.network_status = "Cleared network message queues.";
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

    DrawRecentOrderedMessages(session);
    DrawMessageLog(session);
    ImGui::End();
}

} // namespace splonks::debug_playback_internal

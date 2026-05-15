#include "simulation_snapshot.hpp"
#include "stage_acoustics.hpp"
#include "stage_lighting.hpp"

#include <algorithm>

namespace splonks {

GameplaySnapshot MakeGameplaySnapshot(const State& state, const Graphics& graphics) {
    GameplaySnapshot snapshot;
    snapshot.mode = state.mode;
    snapshot.settings = state.settings;
    snapshot.menu_inputs = state.menu_inputs;
    snapshot.menu_input_snapshot = state.menu_input_snapshot;
    snapshot.previous_menu_input_snapshot = state.previous_menu_input_snapshot;
    snapshot.menu_input_debounce_timers = state.menu_input_debounce_timers;
    snapshot.playing_inputs = state.playing_inputs;
    snapshot.immediate_playing_inputs = state.immediate_playing_inputs;
    snapshot.playing_input_snapshot = state.playing_input_snapshot;
    snapshot.previous_playing_input_snapshot = state.previous_playing_input_snapshot;
    snapshot.previous_immediate_playing_input_snapshot =
        state.previous_immediate_playing_input_snapshot;
    snapshot.title_menu_selection = state.title_menu_selection;
    snapshot.settings_menu_selection = state.settings_menu_selection;
    snapshot.video_settings_menu_selection = state.video_settings_menu_selection;
    snapshot.ui_settings_menu_selection = state.ui_settings_menu_selection;
    snapshot.post_fx_settings_menu_selection = state.post_fx_settings_menu_selection;
    snapshot.lighting_settings_menu_selection = state.lighting_settings_menu_selection;
    snapshot.video_settings_target_window_size_index = state.video_settings_target_window_size_index;
    snapshot.video_settings_target_resolution_index = state.video_settings_target_resolution_index;
    snapshot.video_settings_target_fullscreen = state.video_settings_target_fullscreen;
    snapshot.rebuild_render_texture = state.rebuild_render_texture;
    snapshot.choosing_control_binding = state.choosing_control_binding;
    snapshot.debug_overlay = state.debug_overlay;
    snapshot.debug_shake_brush = state.debug_shake_brush;
    snapshot.debug_audio_brush = state.debug_audio_brush;
    snapshot.debug_fluid_brush = state.debug_fluid_brush;
    snapshot.debug_local_player_bots = state.debug_local_player_bots;
    snapshot.debug_input_override = state.debug_input_override;
    snapshot.next_debug_local_player_id = state.next_debug_local_player_id;
    snapshot.stage_rotation = state.stage_rotation;
    snapshot.player_tuning = state.player_tuning;
    snapshot.running = state.running;
    snapshot.now = state.now;
    snapshot.time_since_last_update = state.time_since_last_update;
    snapshot.scene_frame = state.scene_frame;
    snapshot.frame = state.frame;
    snapshot.stage_frame = state.stage_frame;
    snapshot.drng = state.drng;
    snapshot.stagegen_drng = state.stagegen_drng;
    snapshot.menu_return_to = state.menu_return_to;
    snapshot.game_over = state.game_over;
    snapshot.pause = state.pause;
    snapshot.win = state.win;
    snapshot.respawn_target = state.respawn_target;
    snapshot.pending_stage_transition = state.pending_stage_transition;
    snapshot.multiplayer_respawn_mode = state.multiplayer_respawn_mode;
    snapshot.points = state.points;
    snapshot.deaths = state.deaths;
    snapshot.depth = state.depth;
    snapshot.sac_altar_favor = state.sac_altar_favor;
    snapshot.sac_altar_reward_tier = state.sac_altar_reward_tier;
    snapshot.audio_occlusion_enabled = state.audio_occlusion_enabled;
    snapshot.audio_listener_world_pos = state.audio_listener_world_pos;
    snapshot.gameplay_camera_anchor_world_pos = state.gameplay_camera_anchor_world_pos;
    snapshot.interact_claimed_vids_this_frame = state.interact_claimed_vids_this_frame;
    snapshot.quest_state = state.quest_state;
    snapshot.players = state.players;
    snapshot.frame_pause = state.frame_pause;
    snapshot.debug_level = state.debug_level;
    snapshot.ents = state.ents;
    snapshot.particles = state.particles;
    snapshot.audio_emitters = state.audio_emitters;
    snapshot.area_listener_vids = state.area_listener_vids;
    snapshot.stage = state.stage;
    snapshot.stage_acoustics = state.stage_acoustics;
    snapshot.stage_lighting = state.stage_lighting;
    snapshot.controlled_ent_vid = state.controlled_ent_vid;
    snapshot.spectator_target_player_id = state.spectator_target_player_id;
    snapshot.mouse_trailer_vid = state.mouse_trailer_vid;
    snapshot.contact = state.contact;
    snapshot.ent_tool_states = state.ent_tools.tool_states;
    snapshot.world_prompts = state.world_prompts;
    snapshot.debug_rect_annotations = state.debug_rect_annotations;
    snapshot.debug_label_annotations = state.debug_label_annotations;
    snapshot.play_cam_pos = graphics.play_cam.pos;
    return snapshot;
}

void RestoreGameplaySnapshot(const GameplaySnapshot& snapshot, State& state, Graphics& graphics) {
    state.mode = snapshot.mode;
    state.settings = snapshot.settings;
    state.player_tuning = state.settings.player_tuning;
    state.menu_inputs = snapshot.menu_inputs;
    state.menu_input_snapshot = snapshot.menu_input_snapshot;
    state.previous_menu_input_snapshot = snapshot.previous_menu_input_snapshot;
    state.menu_input_debounce_timers = snapshot.menu_input_debounce_timers;
    state.playing_inputs = snapshot.playing_inputs;
    state.immediate_playing_inputs = snapshot.immediate_playing_inputs;
    state.playing_input_snapshot = snapshot.playing_input_snapshot;
    state.previous_playing_input_snapshot = snapshot.previous_playing_input_snapshot;
    state.previous_immediate_playing_input_snapshot =
        snapshot.previous_immediate_playing_input_snapshot;
    state.title_menu_selection = snapshot.title_menu_selection;
    state.settings_menu_selection = snapshot.settings_menu_selection;
    state.video_settings_menu_selection = snapshot.video_settings_menu_selection;
    state.ui_settings_menu_selection = snapshot.ui_settings_menu_selection;
    state.post_fx_settings_menu_selection = snapshot.post_fx_settings_menu_selection;
    state.lighting_settings_menu_selection = snapshot.lighting_settings_menu_selection;
    state.video_settings_target_window_size_index = snapshot.video_settings_target_window_size_index;
    state.video_settings_target_resolution_index = snapshot.video_settings_target_resolution_index;
    state.video_settings_target_fullscreen = snapshot.video_settings_target_fullscreen;
    state.rebuild_render_texture = snapshot.rebuild_render_texture;
    state.choosing_control_binding = snapshot.choosing_control_binding;
    state.debug_overlay = snapshot.debug_overlay;
    state.debug_shake_brush = snapshot.debug_shake_brush;
    state.debug_audio_brush = snapshot.debug_audio_brush;
    state.debug_fluid_brush = snapshot.debug_fluid_brush;
    state.debug_local_player_bots = snapshot.debug_local_player_bots;
    state.debug_input_override = snapshot.debug_input_override;
    state.next_debug_local_player_id = snapshot.next_debug_local_player_id;
    state.stage_rotation = snapshot.stage_rotation;
    state.player_tuning = snapshot.player_tuning;
    state.running = snapshot.running;
    state.now = snapshot.now;
    state.time_since_last_update = snapshot.time_since_last_update;
    state.scene_frame = snapshot.scene_frame;
    state.frame = snapshot.frame;
    state.stage_frame = snapshot.stage_frame;
    state.drng = snapshot.drng;
    state.stagegen_drng = snapshot.stagegen_drng;
    state.menu_return_to = snapshot.menu_return_to;
    state.game_over = snapshot.game_over;
    state.pause = snapshot.pause;
    state.win = snapshot.win;
    state.respawn_target = snapshot.respawn_target;
    state.pending_stage_transition = snapshot.pending_stage_transition;
    state.multiplayer_respawn_mode = snapshot.multiplayer_respawn_mode;
    state.points = snapshot.points;
    state.deaths = snapshot.deaths;
    state.depth = snapshot.depth;
    state.sac_altar_favor = snapshot.sac_altar_favor;
    state.sac_altar_reward_tier = snapshot.sac_altar_reward_tier;
    state.audio_occlusion_enabled = snapshot.audio_occlusion_enabled;
    state.audio_listener_world_pos = snapshot.audio_listener_world_pos;
    state.gameplay_camera_anchor_world_pos = snapshot.gameplay_camera_anchor_world_pos;
    state.interact_claimed_vids_this_frame = snapshot.interact_claimed_vids_this_frame;
    state.quest_state = snapshot.quest_state;
    state.players = snapshot.players;
    state.frame_pause = snapshot.frame_pause;
    state.debug_level = snapshot.debug_level;
    state.ents = snapshot.ents;
    state.particles = snapshot.particles;
    state.audio_emitters = snapshot.audio_emitters;
    state.area_listener_vids = snapshot.area_listener_vids;
    state.stage = snapshot.stage;
    state.stage_acoustics = snapshot.stage_acoustics;
    state.stage_lighting = snapshot.stage_lighting;
    state.controlled_ent_vid = snapshot.controlled_ent_vid;
    state.spectator_target_player_id = snapshot.spectator_target_player_id;
    state.mouse_trailer_vid = snapshot.mouse_trailer_vid;
    state.contact = snapshot.contact;
    state.ent_tools.tool_states = snapshot.ent_tool_states;
    state.world_prompts = snapshot.world_prompts;
    state.debug_rect_annotations = snapshot.debug_rect_annotations;
    state.debug_label_annotations = snapshot.debug_label_annotations;
    state.RebuildSid(graphics);
    graphics.play_cam.pos = snapshot.play_cam_pos;
}

namespace {

SimPlayerSlotSnapshot MakeSimPlayerSlotSnapshot(const PlayerSlot& slot) {
    SimPlayerSlotSnapshot snapshot;
    snapshot.player_id = slot.player_id;
    snapshot.ent_vid = slot.ent_vid;
    snapshot.connected = slot.connected;
    snapshot.display_name = slot.display_name;
    snapshot.input_frame = slot.input_frame;
    snapshot.previous_input_frame = slot.previous_input_frame;
    snapshot.inputs = slot.inputs;
    snapshot.immediate_inputs = slot.immediate_inputs;
    return snapshot;
}

void RestorePlayerRegistryFromSimSnapshot(
    const std::vector<SimPlayerSlotSnapshot>& snapshots,
    PlayerRegistry& players
) {
    players.slots.clear();
    players.slots.reserve(snapshots.size());
    for (const SimPlayerSlotSnapshot& snapshot : snapshots) {
        PlayerSlot slot;
        slot.player_id = snapshot.player_id;
        slot.ent_vid = snapshot.ent_vid;
        slot.connection_kind = PlayerConnectionKind::Remote;
        slot.connected = snapshot.connected;
        slot.primary_local = false;
        slot.display_name = snapshot.display_name;
        slot.input_frame = snapshot.input_frame;
        slot.previous_input_frame = snapshot.previous_input_frame;
        slot.inputs = snapshot.inputs;
        slot.immediate_inputs = snapshot.immediate_inputs;
        players.slots.push_back(slot);
    }
}

} // namespace

SimSnapshot MakeSimSnapshot(const State& state) {
    SimSnapshot snapshot;
    snapshot.mode = state.mode;
    snapshot.settings = state.settings;
    snapshot.playing_inputs = state.playing_inputs;
    snapshot.immediate_playing_inputs = state.immediate_playing_inputs;
    snapshot.playing_input_snapshot = state.playing_input_snapshot;
    snapshot.previous_playing_input_snapshot = state.previous_playing_input_snapshot;
    snapshot.previous_immediate_playing_input_snapshot =
        state.previous_immediate_playing_input_snapshot;
    snapshot.stage_rotation = state.stage_rotation;
    snapshot.player_tuning = state.player_tuning;
    snapshot.running = state.running;
    snapshot.now = state.now;
    snapshot.time_since_last_update = state.time_since_last_update;
    snapshot.scene_frame = state.scene_frame;
    snapshot.frame = state.frame;
    snapshot.stage_frame = state.stage_frame;
    snapshot.drng = state.drng;
    snapshot.stagegen_drng = state.stagegen_drng;
    snapshot.menu_return_to = state.menu_return_to;
    snapshot.game_over = state.game_over;
    snapshot.pause = state.pause;
    snapshot.win = state.win;
    snapshot.respawn_target = state.respawn_target;
    snapshot.pending_stage_transition = state.pending_stage_transition;
    snapshot.multiplayer_respawn_mode = state.multiplayer_respawn_mode;
    snapshot.points = state.points;
    snapshot.deaths = state.deaths;
    snapshot.depth = state.depth;
    snapshot.sac_altar_favor = state.sac_altar_favor;
    snapshot.sac_altar_reward_tier = state.sac_altar_reward_tier;
    snapshot.interact_claimed_vids_this_frame = state.interact_claimed_vids_this_frame;
    snapshot.quest_state = state.quest_state;
    snapshot.players.reserve(state.players.slots.size());
    for (const PlayerSlot& slot : state.players.slots) {
        snapshot.players.push_back(MakeSimPlayerSlotSnapshot(slot));
    }
    snapshot.frame_pause = state.frame_pause;
    snapshot.debug_level = state.debug_level;
    snapshot.ents = state.ents;
    snapshot.area_listener_vids = state.area_listener_vids;
    snapshot.stage = state.stage;
    snapshot.contact = state.contact;
    snapshot.ent_tool_states = state.ent_tools.tool_states;
    snapshot.net_next_local_ent_id = state.net_session.next_local_ent_id;
    snapshot.net_ent_links.reserve(state.net_session.ent_links.size());
    for (const network::NetEntLink& link : state.net_session.ent_links) {
        snapshot.net_ent_links.push_back(SimNetEntLinkSnapshot{
            .net_id = link.net_id,
            .local_vid = link.local_vid,
            .has_input_owner = link.input_owner_player_id.has_value(),
            .input_owner_player_id =
                link.input_owner_player_id.value_or(kInvalidPlayerId),
        });
    }
    snapshot.net_ent_id_aliases.reserve(state.net_session.ent_id_aliases.size());
    for (const network::NetEntIdAlias& alias : state.net_session.ent_id_aliases) {
        snapshot.net_ent_id_aliases.push_back(SimNetEntIdAliasSnapshot{
            .from_id = alias.from_id,
            .to_id = alias.to_id,
        });
    }
    return snapshot;
}

LocalOverlaySnapshot CaptureLocalOverlaySnapshot(const State& state) {
    LocalOverlaySnapshot snapshot;
    for (const PlayerSlot& slot : state.players.slots) {
        if (slot.connection_kind == PlayerConnectionKind::Local &&
            slot.player_id != kInvalidPlayerId) {
            snapshot.local_player_ids.push_back(slot.player_id);
            if (slot.primary_local) {
                snapshot.primary_local_player_id = slot.player_id;
            }
        }
    }
    snapshot.spectator_target_player_id = state.spectator_target_player_id;
    snapshot.debug_local_player_bots = state.debug_local_player_bots;
    snapshot.debug_input_override = state.debug_input_override;
    return snapshot;
}

void RestoreLocalOverlaySnapshot(
    const LocalOverlaySnapshot& snapshot,
    State& state,
    const Graphics& /*graphics*/
) {
    for (PlayerSlot& slot : state.players.slots) {
        const bool local =
            std::find(
                snapshot.local_player_ids.begin(),
                snapshot.local_player_ids.end(),
                slot.player_id
            ) != snapshot.local_player_ids.end();
        slot.connection_kind = local ? PlayerConnectionKind::Local : PlayerConnectionKind::Remote;
        slot.primary_local = local && slot.player_id == snapshot.primary_local_player_id;
    }

    state.controlled_ent_vid.reset();
    state.spectator_target_player_id = snapshot.spectator_target_player_id;
    if (const PlayerSlot* const primary = state.players.Find(snapshot.primary_local_player_id)) {
        if (primary->connection_kind == PlayerConnectionKind::Local &&
            primary->ent_vid.has_value()) {
            if (const Ent* const ent = state.ents.GetEnt(*primary->ent_vid)) {
                if (ent->active) {
                    state.controlled_ent_vid = primary->ent_vid;
                }
            }
        }
    }

    state.debug_local_player_bots = snapshot.debug_local_player_bots;
    state.debug_input_override = snapshot.debug_input_override;
}

void RestoreSimSnapshot(const SimSnapshot& snapshot, State& state, Graphics& graphics) {
    const LocalOverlaySnapshot local_overlay = CaptureLocalOverlaySnapshot(state);
    state.mode = snapshot.mode;
    state.settings = snapshot.settings;
    state.player_tuning = state.settings.player_tuning;
    state.playing_inputs = snapshot.playing_inputs;
    state.immediate_playing_inputs = snapshot.immediate_playing_inputs;
    state.playing_input_snapshot = snapshot.playing_input_snapshot;
    state.previous_playing_input_snapshot = snapshot.previous_playing_input_snapshot;
    state.previous_immediate_playing_input_snapshot =
        snapshot.previous_immediate_playing_input_snapshot;
    state.stage_rotation = snapshot.stage_rotation;
    state.player_tuning = snapshot.player_tuning;
    state.running = snapshot.running;
    state.now = snapshot.now;
    state.time_since_last_update = snapshot.time_since_last_update;
    state.scene_frame = snapshot.scene_frame;
    state.frame = snapshot.frame;
    state.stage_frame = snapshot.stage_frame;
    state.drng = snapshot.drng;
    state.stagegen_drng = snapshot.stagegen_drng;
    state.menu_return_to = snapshot.menu_return_to;
    state.game_over = snapshot.game_over;
    state.pause = snapshot.pause;
    state.win = snapshot.win;
    state.respawn_target = snapshot.respawn_target;
    state.pending_stage_transition = snapshot.pending_stage_transition;
    state.multiplayer_respawn_mode = snapshot.multiplayer_respawn_mode;
    state.points = snapshot.points;
    state.deaths = snapshot.deaths;
    state.depth = snapshot.depth;
    state.sac_altar_favor = snapshot.sac_altar_favor;
    state.sac_altar_reward_tier = snapshot.sac_altar_reward_tier;
    state.interact_claimed_vids_this_frame = snapshot.interact_claimed_vids_this_frame;
    state.quest_state = snapshot.quest_state;
    RestorePlayerRegistryFromSimSnapshot(snapshot.players, state.players);
    state.frame_pause = snapshot.frame_pause;
    state.debug_level = snapshot.debug_level;
    state.ents = snapshot.ents;
    state.area_listener_vids = snapshot.area_listener_vids;
    state.stage = snapshot.stage;
    state.contact = snapshot.contact;
    state.ent_tools.tool_states = snapshot.ent_tool_states;
    state.net_session.next_local_ent_id = snapshot.net_next_local_ent_id;
    state.net_session.ent_links.clear();
    state.net_session.ent_links.reserve(snapshot.net_ent_links.size());
    for (const SimNetEntLinkSnapshot& link : snapshot.net_ent_links) {
        state.net_session.ent_links.push_back(network::NetEntLink{
            .net_id = link.net_id,
            .local_vid = link.local_vid,
            .input_owner_player_id = link.has_input_owner
                ? std::optional<PlayerId>{link.input_owner_player_id}
                : std::nullopt,
        });
    }
    state.net_session.ent_id_aliases.clear();
    state.net_session.ent_id_aliases.reserve(snapshot.net_ent_id_aliases.size());
    for (const SimNetEntIdAliasSnapshot& alias : snapshot.net_ent_id_aliases) {
        state.net_session.ent_id_aliases.push_back(network::NetEntIdAlias{
            .from_id = alias.from_id,
            .to_id = alias.to_id,
        });
    }
    state.world_prompts.clear();
    state.debug_rect_annotations.clear();
    state.debug_label_annotations.clear();
    RestoreLocalOverlaySnapshot(local_overlay, state, graphics);
    state.RebuildSid(graphics);
}

} // namespace splonks

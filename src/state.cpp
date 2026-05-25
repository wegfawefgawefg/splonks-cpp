#include "state.hpp"

#include "ents/common/common.hpp"
#include "quest_stage_loader.hpp"
#include "stage_init.hpp"
#include "world_query.hpp"

#include <algorithm>
#include <cmath>

namespace splonks {

namespace {

bool HasAnyAreaListenerCallback(const Ent& ent) {
    return ent.on_area_enter != nullptr || ent.on_area_exit != nullptr ||
           ent.on_area_tile_changed != nullptr;
}

} // namespace

void AddShake(State& state, const Vec2& world_pos, float foreground_tile_amount,
              float background_tile_amount, float ent_amount, float radius_tiles,
              std::optional<VID> exclude_ent_vid) {
    if (foreground_tile_amount <= 0.0F && background_tile_amount <= 0.0F && ent_amount <= 0.0F) {
        return;
    }

    const IVec2 world_pixel = IVec2::New(static_cast<int>(std::floor(world_pos.x)),
                                         static_cast<int>(std::floor(world_pos.y)));
    const IVec2 tile_pos = state.stage.GetTileCoordAtWc(world_pixel);

    if (foreground_tile_amount > 0.0F) {
        state.stage.AddForegroundTileShakeArea(tile_pos, foreground_tile_amount, radius_tiles);
    }
    if (background_tile_amount > 0.0F) {
        state.stage.AddBackgroundTileShakeArea(tile_pos, background_tile_amount, radius_tiles);
    }
    if (ent_amount <= 0.0F) {
        return;
    }

    const float radius_world = radius_tiles * static_cast<float>(kTileSize);
    const AABB area = AABB::New(world_pos - Vec2::New(radius_world, radius_world),
                                world_pos + Vec2::New(radius_world, radius_world));
    for (const VID& vid : QueryEntsInAabb(state, area, exclude_ent_vid)) {
        Ent* const ent = state.ents.GetEntMut(vid);
        if (ent == nullptr || !ent->active) {
            continue;
        }

        const Vec2 nearest_center = GetNearestWorldPoint(state.stage, world_pos, ent->GetCenter());
        const Vec2 delta = nearest_center - world_pos;
        const float distance = std::sqrt((delta.x * delta.x) + (delta.y * delta.y));
        if (radius_world > 0.0F) {
            if (distance > radius_world) {
                continue;
            }
            AddEntShake(*ent, ent_amount * (1.0F - (distance / radius_world)));
            continue;
        }

        const AABB nearest_aabb = GetNearestWorldAabb(state.stage, world_pos, ent->GetAABB());
        if (world_pos.x >= nearest_aabb.tl.x && world_pos.x <= nearest_aabb.br.x &&
            world_pos.y >= nearest_aabb.tl.y && world_pos.y <= nearest_aabb.br.y) {
            AddEntShake(*ent, ent_amount);
        }
    }
}

void AddShake(State& state, const Vec2& world_pos, float amount, float radius_tiles,
              std::optional<VID> exclude_ent_vid) {
    AddShake(state, world_pos, amount, amount, amount, radius_tiles, exclude_ent_vid);
}

void AddShake(State& state, const Vec2& world_pos, float amount, float radius_tiles, ShakeMask mask,
              std::optional<VID> exclude_ent_vid) {
    if (amount <= 0.0F || mask == ShakeMask::None) {
        return;
    }

    AddShake(state, world_pos, HasShakeMask(mask, ShakeMask::ForegroundTiles) ? amount : 0.0F,
             HasShakeMask(mask, ShakeMask::BackgroundTiles) ? amount : 0.0F,
             HasShakeMask(mask, ShakeMask::Ents) ? amount : 0.0F, radius_tiles, exclude_ent_vid);
}

State State::New() {
    State state;
    state.mode = Mode::Title;
    state.settings = LoadSettings();
    state.player_tuning = state.settings.player_tuning;
    state.menu_inputs = MenuInputs::New();
    state.menu_input_snapshot = MenuInputSnapshot::New();
    state.previous_menu_input_snapshot = MenuInputSnapshot::New();
    state.menu_input_debounce_timers = MenuInputDebounceTimers::New();
    state.playing_inputs = PlayingInputs::New();
    state.immediate_playing_inputs = PlayingInputs::New();
    state.playing_input_snapshot = PlayingInputSnapshot::New();
    state.previous_playing_input_snapshot = PlayingInputSnapshot::New();
    state.previous_immediate_playing_input_snapshot = PlayingInputSnapshot::New();
    state.external_local_input_frames.clear();
    state.use_external_local_input_frames = false;
    state.title_menu_selection = TitleMenuOption::Start;
    state.gubsy_shell_ui_active = false;
    state.settings_menu_selection = SettingsMenuOption::Video;
    state.video_settings_menu_selection = VideoSettingsMenuOption::Resolution;
    state.ui_settings_menu_selection = UiSettingsMenuOption::IconScale;
    state.post_fx_settings_menu_selection = PostFxSettingsMenuOption::Effect;
    state.lighting_settings_menu_selection = LightingSettingsMenuOption::TerrainLighting;
    state.video_settings_target_window_size_index.reset();
    state.video_settings_target_resolution_index.reset();
    state.video_settings_target_fullscreen.reset();
    state.choosing_control_binding = false;
    state.suppress_gameplay_input = false;
    state.gameplay_input_suppression_frames = 0;
    state.rebuild_render_texture = false;
    state.scene_frame = 0;
    state.time_since_last_update = 0.0F;
    state.now = 0.0;
    state.running = true;
    state.frame = 0;
    state.stage_frame = 0;
    state.drng = DetRng::New(1);
    state.stagegen_drng = DetRng::New(1);
    state.debug_primary_player_bot_enabled = false;
    state.debug_primary_player_bot = DebugLocalPlayerBot{};
    state.debug_primary_player_bot.player_id = kInvalidPlayerId;
    state.menu_return_to = Mode::Title;
    state.game_over = false;
    state.pause = false;
    state.win = false;
    state.points = 0;
    state.deaths = 0;
    state.depth = 0;
    state.sac_altar_favor = 0;
    state.sac_altar_reward_tier = 0;
    state.frame_pause = 0;
    state.audio_occlusion_enabled = true;
    state.audio_listener_world_pos = Vec2::New(0.0F, 0.0F);
    state.gameplay_camera_anchor_world_pos.reset();
    state.interact_claimed_vids_this_frame.clear();
    state.players = PlayerRegistry::New();
    state.net_session = network::NetSessionState::NewOffline();
    state.net_transport.reset();
    state.ents = EntPool::New();
    state.particles = ParticleSystem{};
    state.audio_emitters = AudioEmitterManager::New();
    state.sid = SID::New();
    state.area_listener_vids.clear();
    state.respawn_target = StageLoadTarget::ForQuestStage("classic", "classic_mines_1");
    state.pending_stage_transition.reset();
    state.stage_lighting = StageLighting::New();
    state.stage_acoustics = StageAcoustics::New();
    state.controlled_ent_vid.reset();
    state.spectator_target_player_id.reset();
    state.mouse_trailer_vid.reset();
    state.contact = ContactBookkeeping{};
    state.ent_tools = EntToolInventoryState{};
    state.world_prompts.clear();
    state.debug_rect_annotations.clear();
    state.debug_label_annotations.clear();
    state.debug_local_player_bots.clear();
    state.next_debug_local_player_id = 2;
    state.debug_fluid_brush.enabled = state.settings.debug_ui.fluid_brush_enabled;
    state.debug_fluid_brush.replace_solid_tiles =
        state.settings.debug_ui.fluid_brush_replace_solid_tiles;
    state.debug_fluid_brush.mode = static_cast<DebugFluidBrushState::Mode>(
        std::clamp(state.settings.debug_ui.fluid_brush_mode, 0, 3));
    state.debug_fluid_brush.radius_tiles =
        std::max(0, state.settings.debug_ui.fluid_brush_radius_tiles);
    state.debug_fluid_brush.paint_gravity_x = state.settings.debug_ui.fluid_brush_paint_gravity_x;
    state.debug_fluid_brush.paint_gravity_y = state.settings.debug_ui.fluid_brush_paint_gravity_y;
    state.debug_fluid_brush.show_flow_indicators =
        state.settings.debug_ui.fluid_brush_show_flow_indicators;
    (void)LoadQuestStage(state, "classic", "classic_mines_1", false);
    return state;
}

void State::SetMode(Mode new_mode) {
    mode = new_mode;
    if (new_mode != Mode::GameOver) {
        gameplay_camera_anchor_world_pos.reset();
    }
    scene_frame = 0;
    world_prompts.clear();
    debug_rect_annotations.clear();
    debug_label_annotations.clear();
    interact_claimed_vids_this_frame.clear();
}

void State::RebuildSid(const Graphics& graphics) {
    sid.Clear();
    area_listener_vids.clear();

    for (std::size_t ent_id = 0; ent_id < ents.ents.size(); ++ent_id) {
        UpdateSidForEnt(ent_id, graphics);
    }
}

void State::UpdateSidForEnt(std::size_t ent_id, const Graphics& graphics) {
    if (ent_id >= ents.ents.size()) {
        return;
    }

    const Ent& ent = ents.ents[ent_id];
    UpdateAreaListenerCacheForEnt(ent_id);
    sid.Remove(ent.vid);
    if (!ent.active) {
        return;
    }

    const AABB broadphase_aabb = ents::common::GetEntBroadphaseAabb(ent, graphics);
    sid.Upsert(ent.vid, broadphase_aabb);
}

void State::RebuildAreaListenerCache() {
    area_listener_vids.clear();
    for (std::size_t ent_id = 0; ent_id < ents.ents.size(); ++ent_id) {
        UpdateAreaListenerCacheForEnt(ent_id);
    }
}

void State::UpdateAreaListenerCacheForEnt(std::size_t ent_id) {
    if (ent_id >= ents.ents.size()) {
        return;
    }

    const Ent& ent = ents.ents[ent_id];
    area_listener_vids.erase(
        std::remove_if(area_listener_vids.begin(), area_listener_vids.end(),
                       [&](const VID& candidate) { return candidate.id == ent.vid.id; }),
        area_listener_vids.end());

    if (!ent.active || !HasAnyAreaListenerCallback(ent)) {
        return;
    }

    area_listener_vids.push_back(ent.vid);
}

void State::ClearWorldPrompts() {
    world_prompts.clear();
}

void State::AddWorldPrompt(const WorldPrompt& prompt) {
    world_prompts.push_back(prompt);
}

void State::ClearDebugAnnotations() {
    debug_rect_annotations.clear();
    debug_label_annotations.clear();
}

void State::AddDebugRectAnnotation(const DebugRectAnnotation& annotation) {
    debug_rect_annotations.push_back(annotation);
}

void State::AddDebugLabelAnnotation(const DebugLabelAnnotation& annotation) {
    debug_label_annotations.push_back(annotation);
}

void State::ClearInteractClaims() {
    interact_claimed_vids_this_frame.clear();
}

void State::ClaimInteractForEnt(VID ent_vid) {
    if (std::find(interact_claimed_vids_this_frame.begin(), interact_claimed_vids_this_frame.end(),
                  ent_vid) == interact_claimed_vids_this_frame.end()) {
        interact_claimed_vids_this_frame.push_back(ent_vid);
    }
}

bool State::IsInteractClaimedForEnt(VID ent_vid) const {
    return std::find(interact_claimed_vids_this_frame.begin(),
                     interact_claimed_vids_this_frame.end(),
                     ent_vid) != interact_claimed_vids_this_frame.end();
}

} // namespace splonks

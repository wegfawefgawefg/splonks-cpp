#pragma once

#include "audio.hpp"
#include "graphics.hpp"
#include "state.hpp"

#include <SDL3/SDL.h>

#include <array>
#include <cstddef>
#include <deque>
#include <optional>
#include <string>

namespace splonks {

struct GameplaySnapshot {
    Mode mode = Mode::Title;
    Settings settings;
    MenuInputs menu_inputs;
    MenuInputSnapshot menu_input_snapshot;
    MenuInputSnapshot previous_menu_input_snapshot;
    MenuInputDebounceTimers menu_input_debounce_timers;
    PlayingInputs playing_inputs;
    PlayingInputs immediate_playing_inputs;
    PlayingInputSnapshot playing_input_snapshot;
    PlayingInputSnapshot previous_playing_input_snapshot;
    PlayingInputSnapshot previous_immediate_playing_input_snapshot;
    TitleMenuOption title_menu_selection = TitleMenuOption::Start;
    SettingsMenuOption settings_menu_selection = SettingsMenuOption::Video;
    VideoSettingsMenuOption video_settings_menu_selection = VideoSettingsMenuOption::Resolution;
    UiSettingsMenuOption ui_settings_menu_selection = UiSettingsMenuOption::IconScale;
    PostFxSettingsMenuOption post_fx_settings_menu_selection = PostFxSettingsMenuOption::Effect;
    LightingSettingsMenuOption lighting_settings_menu_selection =
        LightingSettingsMenuOption::TerrainFaceShading;
    std::optional<std::size_t> video_settings_target_window_size_index;
    std::optional<std::size_t> video_settings_target_resolution_index;
    std::optional<bool> video_settings_target_fullscreen;
    bool rebuild_render_texture = false;
    bool choosing_control_binding = false;
    DebugOverlayState debug_overlay;
    DebugShakeBrushState debug_shake_brush;
    double now = 0.0;
    float time_since_last_update = 0.0F;
    std::uint32_t scene_frame = 0;
    std::uint32_t frame = 0;
    std::uint32_t stage_frame = 0;
    Mode menu_return_to = Mode::Title;
    bool game_over = false;
    bool pause = false;
    bool win = false;
    StageLoadTarget respawn_target = StageLoadTarget::ForStageType(StageType::SplkMines1);
    std::optional<StageTransitionTarget> pending_stage_transition;
    std::uint32_t points = 0;
    std::uint32_t deaths = 0;
    std::uint32_t frame_pause = 0;
    DebugLevelConfig debug_level;
    EntityManager entity_manager;
    Stage stage;
    std::optional<VID> player_vid;
    std::optional<VID> controlled_entity_vid;
    std::optional<VID> mouse_trailer_vid;
    std::vector<EntityToolState> entity_tool_states;
    Vec2 play_cam_pos;
};

struct DebugPlayback {
    bool imgui_visible = true;
    bool ui_visible = true;
    bool playback_window_visible = true;
    bool level_window_visible = true;
    bool border_window_visible = true;
    bool entity_inspector_visible = true;
    bool entity_annotations_visible = false;
    bool shake_brush_window_visible = false;
    bool ui_settings_window_visible = false;
    bool post_fx_settings_window_visible = false;
    bool lighting_settings_window_visible = false;
    bool graphics_settings_window_visible = false;
    bool camera_settings_window_visible = false;
    bool recording = false;
    bool playback_active = false;
    bool pause_live_simulation = false;
    bool step_live_simulation_once = false;
    bool skip_live_simulation_once = false;
    float time_scale = 1.0F;
    int max_snapshots = 1200;
    std::deque<GameplaySnapshot> recorded_snapshots;
    std::optional<GameplaySnapshot> live_resume_snapshot;
    std::size_t playback_index = 0;
    std::size_t selected_entity_id = 0;
    EntityType spawn_entity_type = EntityType::JetPack;
    std::array<char, 64> spawn_search{};
    bool spawn_center_on_selected = false;
    bool spawn_held_by_player = false;
    bool pending_spawn_at_mouse = false;
    double pending_spawn_at_mouse_until = 0.0;
    std::string spawn_status;
    std::array<char, 512> file_path{};
    std::string io_status;

    static DebugPlayback New();
};

GameplaySnapshot MakeGameplaySnapshot(const State& state, const Graphics& graphics);
void RestoreGameplaySnapshot(const GameplaySnapshot& snapshot, State& state, Graphics& graphics);

void DrawDebugPlaybackControls(
    DebugPlayback& debug,
    State& state,
    Graphics& graphics,
    SDL_Window* window,
    SDL_Renderer* renderer
);
void DrawDebugPlaybackInspector(DebugPlayback& debug, State& state, const Graphics& graphics);
bool ConvertRecordingFileToText(
    const std::string& input_path,
    const std::string& output_path,
    const FrameDataDb& frame_data_db,
    std::string* status_out
);
void RunSimulationWithDebugControls(
    SDL_Window* window,
    SDL_Renderer* renderer,
    State& state,
    Audio& audio,
    Graphics& graphics,
    DebugPlayback& debug,
    float frame_dt
);

} // namespace splonks

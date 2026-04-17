#pragma once

#include "contact_bookkeeping.hpp"
#include "entity/manager.hpp"
#include "entity_tool_inventory.hpp"
#include "inputs.hpp"
#include "menu/settings.hpp"
#include "menu/postfx.hpp"
#include "menu/lighting.hpp"
#include "menu/title.hpp"
#include "menu/ui.hpp"
#include "menu/video.hpp"
#include "settings.hpp"
#include "sid.hpp"
#include "particles/system.hpp"
#include "tools/tool_archetype.hpp"
#include "stage.hpp"
#include "stage_progression.hpp"
#include "stage_lighting.hpp"

#include <cstdint>
#include <array>
#include <memory>
#include <optional>
#include <vector>

namespace splonks {

struct Graphics;

enum class Mode {
    Title,
    Settings,
    VideoSettings,
    UiSettings,
    PostFxSettings,
    LightingSettings,
    Playing,
    StageTransition,
    GameOver,
    Win,
};

constexpr std::uint32_t kStageSettleFrames = 100;

struct HangTestLevelConfig {
    int stage_height_tiles = 128;
    int cutout_drop_tiles = 8;
    int cutout_height_tiles = 2;
};

enum class MazeDoorTestRoom : std::uint8_t {
    RoomA,
    RoomB,
    RoomC,
};

struct BorderTestLevelConfig {
    Tile left_tile = Tile::Air;
    Tile right_tile = Tile::Air;
    Tile top_tile = Tile::Air;
    Tile bottom_tile = Tile::Air;
    bool wrap_x = false;
    bool wrap_y = false;
    int wrap_padding_chunks = 0;
    bool camera_clamp_enabled = true;
    std::optional<int> void_death_y = std::nullopt;
};

struct MazeDoorTestLevelConfig {
    MazeDoorTestRoom room = MazeDoorTestRoom::RoomA;
};

struct DebugLevelConfig {
    DebugLevelKind kind = DebugLevelKind::MovingPlatformTest;
    HangTestLevelConfig hang_test;
    BorderTestLevelConfig border_test;
    MazeDoorTestLevelConfig maze_door_test;
};

struct DebugOverlayState {
    bool show_entity_collision_boxes = false;
    bool show_entity_ids = false;
    bool show_entity_types = false;
    bool show_void_death_line = false;
    bool show_chunk_boundaries = false;
    bool show_chunk_coords = false;
    bool show_tile_indexes = false;
    bool show_tile_types = false;
    bool show_lights = false;
    bool show_area_boundaries = false;
    bool show_area_ids = false;
    bool show_area_types = false;
};

struct State {
    // Menu and input state.
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
        LightingSettingsMenuOption::TerrainLighting;
    std::optional<std::size_t> video_settings_target_window_size_index;
    std::optional<std::size_t> video_settings_target_resolution_index;
    std::optional<bool> video_settings_target_fullscreen;
    bool rebuild_render_texture = false;
    bool choosing_control_binding = false;

    // Debug state.
    DebugOverlayState debug_overlay;
    bool running = true;

    // Frame and simulation timing.
    double now = 0.0;
    float time_since_last_update = 0.0F;
    std::uint32_t scene_frame = 0;
    std::uint32_t frame = 0;
    std::uint32_t stage_frame = 0;

    // Session and progression state.
    Mode menu_return_to = Mode::Title;
    bool game_over = false;
    bool pause = false;
    bool win = false;
    StageLoadTarget respawn_target = StageLoadTarget::ForStageType(StageType::SplkMines1);
    std::optional<StageTransitionTarget> pending_stage_transition;
    std::uint32_t points = 0;
    std::uint32_t deaths = 0;
    std::uint32_t frame_pause = 0;

    // World and debug level state.
    DebugLevelConfig debug_level;
    EntityManager entity_manager;
    ParticleSystem particles;
    SID sid;
    std::vector<VID> area_listener_vids;
    Stage stage;
    StageLighting stage_lighting;

    // Common entity references.
    std::optional<VID> player_vid;
    std::optional<VID> controlled_entity_vid;
    std::optional<VID> mouse_trailer_vid;

    // Contact and interaction bookkeeping.
    ContactBookkeeping contact;

    // Per-entity owned tool state.
    EntityToolInventoryState entity_tools;

    static State New();
    void SetMode(Mode new_mode);
    void RebuildSid(const Graphics& graphics);
    void UpdateSidForEntity(std::size_t entity_id, const Graphics& graphics);
    void RebuildAreaListenerCache();
    void UpdateAreaListenerCacheForEntity(std::size_t entity_id);
};

bool IsStageWon(const State& state);

} // namespace splonks

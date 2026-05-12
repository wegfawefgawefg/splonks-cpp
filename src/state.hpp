#pragma once

#include "audio.hpp"
#include "audio_emitters.hpp"
#include "contact_bookkeeping.hpp"
#include "entity/manager.hpp"
#include "entity_tool_inventory.hpp"
#include "frame_data_id.hpp"
#include "gameplay_messages.hpp"
#include "inputs.hpp"
#include "menu/settings.hpp"
#include "menu/postfx.hpp"
#include "menu/lighting.hpp"
#include "menu/title.hpp"
#include "menu/ui.hpp"
#include "menu/video.hpp"
#include "network/net_session.hpp"
#include "network/net_transport.hpp"
#include "player_registry.hpp"
#include "settings.hpp"
#include "sid.hpp"
#include "particles/system.hpp"
#include "tools/tool_archetype.hpp"
#include "stage.hpp"
#include "stage_acoustics.hpp"
#include "stage_progression.hpp"
#include "stage_rotation.hpp"
#include "quest.hpp"
#include "stage_lighting.hpp"
#include "utils.hpp"

#include <cstdint>
#include <array>
#include <memory>
#include <optional>
#include <string>
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

enum class ShakeMask : std::uint8_t {
    None = 0,
    ForegroundTiles = 1 << 0,
    BackgroundTiles = 1 << 1,
    Entities = 1 << 2,
    Tiles = 3,
    All = 7,
};

constexpr ShakeMask operator|(ShakeMask a, ShakeMask b) {
    return static_cast<ShakeMask>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}

constexpr bool HasShakeMask(ShakeMask mask, ShakeMask flag) {
    return (static_cast<std::uint8_t>(mask) & static_cast<std::uint8_t>(flag)) != 0;
}

struct HangTestLevelConfig {
    int drop_tiles = 8;
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
    int wrap_padding_tiles = 0;
    bool camera_clamp_enabled = true;
    std::optional<int> void_death_y = std::nullopt;
};

struct MazeDoorTestLevelConfig {
    MazeDoorTestRoom room = MazeDoorTestRoom::RoomA;
};

struct CrusherTrapTestLevelConfig {
    int stress_squisher_count = 0;
    int squisher_sensor_tiles = 0;
};

struct LightingStressTestLevelConfig {
    int moving_light_count = 128;
};

struct DebugLevelConfig {
    DebugLevelKind kind = DebugLevelKind::HangTest;
    HangTestLevelConfig hang_test;
    BorderTestLevelConfig border_test;
    MazeDoorTestLevelConfig maze_door_test;
    CrusherTrapTestLevelConfig crusher_trap_test;
    LightingStressTestLevelConfig lighting_stress_test;
};

struct DebugOverlayState {
    bool show_entity_collision_boxes = false;
    bool show_entity_ids = false;
    bool show_entity_types = false;
    bool show_entity_render_centers = false;
    bool show_void_death_line = false;
    bool show_chunk_boundaries = false;
    bool show_chunk_coords = false;
    bool show_tile_indexes = false;
    bool show_tile_types = false;
    bool show_tile_openness = false;
    bool show_fluid_amounts = false;
    bool show_fluid_gravity = false;
    bool show_lights = false;
    bool show_area_boundaries = false;
    bool show_area_ids = false;
    bool show_area_types = false;
    bool show_audio_emitters = false;
    bool show_audio_occlusion_paths = false;
    bool show_debug_annotations = false;
    bool show_stagegen_annotations = false;
};

struct DebugAnnotationColor {
    std::uint8_t r = 255;
    std::uint8_t g = 255;
    std::uint8_t b = 255;
    std::uint8_t a = 255;
};

struct DebugRectAnnotation {
    AABB area = AABB::New(Vec2::New(0.0F, 0.0F), Vec2::New(0.0F, 0.0F));
    DebugAnnotationColor color{};
};

struct DebugLabelAnnotation {
    Vec2 world_pos = Vec2::New(0.0F, 0.0F);
    std::string text;
    DebugAnnotationColor color{};
};

struct DebugShakeBrushState {
    bool enabled = false;
    bool affect_foreground_tiles = true;
    bool affect_background_tiles = false;
    bool affect_entities = false;
    float foreground_tile_amount = 1.0F;
    float background_tile_amount = 1.0F;
    float entity_amount = 1.0F;
    float radius_tiles = 2.0F;
};

struct DebugAudioBrushState {
    bool enabled = false;
    bool show_openness_rays = false;
    bool show_occlusion_ray = false;
    AudioAssetId audio_asset_id = audio_asset_ids::BoulderRoll;
    float volume_scale = 1.0F;
    bool source_active = false;
    Vec2 source_world_pos = Vec2::New(0.0F, 0.0F);
};

struct DebugFluidBrushState {
    enum class Mode : std::uint8_t {
        Water,
        PermanentGravity,
        TemporaryGravity,
        GlobalGravityDirection,
    };

    bool enabled = false;
    bool replace_solid_tiles = false;
    Mode mode = Mode::Water;
    int radius_tiles = 1;
    float paint_gravity_x = 0.0F;
    float paint_gravity_y = -2.0F;
    bool show_flow_indicators = false;
};

struct DebugLocalPlayerBot {
    PlayerId player_id = kInvalidPlayerId;
    bool enabled = true;
    bool allow_jump = true;
    bool allow_tools = false;
    int retarget_frames = 0;
    int jump_cooldown_frames = 0;
    int move_dir = 0;
    PlayingInputs previous_inputs = PlayingInputs::New();
};

struct StageRotationState {
    bool active = false;
    int elapsed_frames = 0;
    int duration_frames = 180;
    int quarter_turns = 1;
    Vec2 pivot = Vec2::New(0.0F, 0.0F);
    StageRotationWrapPolicy wrap_policy = StageRotationWrapPolicy::DoNotChangeWrap;
};

struct WorldPrompt {
    Vec2 world_pos = Vec2::New(0.0F, 0.0F);
    const char* action_text = "";
    const char* message_text = "";
    bool show_down_arrow = false;
    std::uint32_t quantity = 0;
    std::optional<FrameDataId> icon_animation_id = std::nullopt;
};

struct PerformanceStats {
    double frame_budget_ms = 1000.0 / 60.0;
    double step_ms = 0.0;
    double render_ms = 0.0;
    double imgui_ms = 0.0;
    double present_ms = 0.0;
    double frame_total_ms = 0.0;
    double step_smoothed_ms = 0.0;
    double render_smoothed_ms = 0.0;
    double imgui_smoothed_ms = 0.0;
    double present_smoothed_ms = 0.0;
    double frame_total_smoothed_ms = 0.0;
    double step_peak_ms = 0.0;
    double render_peak_ms = 0.0;
    double imgui_peak_ms = 0.0;
    double present_peak_ms = 0.0;
    double frame_total_peak_ms = 0.0;
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
    DebugShakeBrushState debug_shake_brush;
    DebugAudioBrushState debug_audio_brush;
    DebugFluidBrushState debug_fluid_brush;
    std::vector<DebugLocalPlayerBot> debug_local_player_bots;
    PlayerId next_debug_local_player_id = 2;
    StageRotationState stage_rotation;
    PlayerTuningState player_tuning;
    bool running = true;

    // Frame and simulation timing.
    double now = 0.0;
    float time_since_last_update = 0.0F;
    std::uint32_t scene_frame = 0;
    std::uint32_t frame = 0;
    std::uint32_t stage_frame = 0;
    DeterministicRng drng;

    // Session and progression state.
    Mode menu_return_to = Mode::Title;
    bool game_over = false;
    bool pause = false;
    bool win = false;
    StageLoadTarget respawn_target = StageLoadTarget::ForQuestStage("classic", "classic_mines_1");
    std::optional<StageTransitionTarget> pending_stage_transition;
    std::uint32_t points = 0;
    std::uint32_t deaths = 0;
    std::uint32_t depth = 0;
    QuestState quest_state;
    std::int32_t sac_altar_favor = 0;
    std::uint32_t sac_altar_reward_tier = 0;
    std::uint32_t frame_pause = 0;
    bool audio_occlusion_enabled = true;
    Vec2 audio_listener_world_pos = Vec2::New(0.0F, 0.0F);
    std::optional<Vec2> gameplay_camera_anchor_world_pos;
    std::vector<GameplayActionRequested> pending_gameplay_actions;
    std::vector<VID> interact_claimed_vids_this_frame;
    PerformanceStats performance_stats;
    PlayerRegistry players;
    network::NetSessionState net_session;
    std::unique_ptr<network::NetTransportRuntime> net_transport;

    // World and debug level state.
    DebugLevelConfig debug_level;
    EntityManager entity_manager;
    ParticleSystem particles;
    AudioEmitterManager audio_emitters;
    SID sid;
    std::vector<VID> area_listener_vids;
    Stage stage;
    StageAcoustics stage_acoustics;
    StageLighting stage_lighting;

    // Common entity references.
    std::optional<VID> controlled_entity_vid;
    std::optional<PlayerId> spectator_target_player_id;
    std::optional<VID> mouse_trailer_vid;

    // Contact and interaction bookkeeping.
    ContactBookkeeping contact;

    // Per-entity owned tool state.
    EntityToolInventoryState entity_tools;
    std::vector<WorldPrompt> world_prompts;
    std::vector<DebugRectAnnotation> debug_rect_annotations;
    std::vector<DebugLabelAnnotation> debug_label_annotations;

    static State New();
    void SetMode(Mode new_mode);
    void RebuildSid(const Graphics& graphics);
    void UpdateSidForEntity(std::size_t entity_id, const Graphics& graphics);
    void RebuildAreaListenerCache();
    void UpdateAreaListenerCacheForEntity(std::size_t entity_id);
    void ClearWorldPrompts();
    void AddWorldPrompt(const WorldPrompt& prompt);
    void ClearDebugAnnotations();
    void AddDebugRectAnnotation(const DebugRectAnnotation& annotation);
    void AddDebugLabelAnnotation(const DebugLabelAnnotation& annotation);
    void ClearInteractClaims();
    void ClaimInteractForEntity(VID entity_vid);
    bool IsInteractClaimedForEntity(VID entity_vid) const;
};

void AddShake(
    State& state,
    const Vec2& world_pos,
    float foreground_tile_amount,
    float background_tile_amount,
    float entity_amount,
    float radius_tiles,
    std::optional<VID> exclude_entity_vid = std::nullopt
);
void AddShake(
    State& state,
    const Vec2& world_pos,
    float amount,
    float radius_tiles,
    std::optional<VID> exclude_entity_vid = std::nullopt
);
void AddShake(
    State& state,
    const Vec2& world_pos,
    float amount,
    float radius_tiles,
    ShakeMask mask,
    std::optional<VID> exclude_entity_vid = std::nullopt
);

} // namespace splonks

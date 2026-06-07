#pragma once

#include "graphics.hpp"
#include "state.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

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
        LightingSettingsMenuOption::TerrainLighting;
    std::optional<std::uint32_t> video_settings_target_window_size_index;
    std::optional<std::uint32_t> video_settings_target_resolution_index;
    std::optional<bool> video_settings_target_fullscreen;
    bool rebuild_render_texture = false;
    bool choosing_control_binding = false;
    DebugOverlayState debug_overlay;
    DebugShakeBrushState debug_shake_brush;
    DebugAudioBrushState debug_audio_brush;
    DebugFluidBrushState debug_fluid_brush;
    std::vector<DebugLocalPlayerBot> debug_local_player_bots;
    DebugInputOverrideState debug_input_override;
    PlayerId next_debug_local_player_id = 2;
    StageRotationState stage_rotation;
    PlayerTuningState player_tuning;
    bool running = true;
    double now = 0.0;
    float time_since_last_update = 0.0F;
    std::uint32_t scene_frame = 0;
    std::uint32_t frame = 0;
    std::uint32_t stage_frame = 0;
    DetRng drng;
    DetRng stagegen_drng;
    Mode menu_return_to = Mode::Title;
    bool game_over = false;
    bool pause = false;
    bool win = false;
    StageLoadTarget respawn_target = StageLoadTarget::ForQuestStage("classic", "classic_mines_1");
    std::optional<StageTransitionTarget> pending_stage_transition;
    MultiplayerRespawnMode multiplayer_respawn_mode = MultiplayerRespawnMode::GenerousNextLevel;
    std::uint32_t points = 0;
    std::uint32_t deaths = 0;
    std::uint32_t depth = 0;
    std::int32_t sac_altar_favor = 0;
    std::uint32_t sac_altar_reward_tier = 0;
    bool audio_occlusion_enabled = true;
    Vec2 audio_listener_world_pos = Vec2::New(0.0F, 0.0F);
    std::optional<Vec2> gameplay_camera_anchor_world_pos;
    std::vector<VID> interact_claimed_vids_this_frame;
    QuestState quest_state;
    PlayerRegistry players;
    std::uint32_t frame_pause = 0;
    DebugLevelConfig debug_level;
    EntPool ents;
    ParticleSystem particles;
    AudioEmitterManager audio_emitters;
    std::vector<VID> area_listener_vids;
    Stage stage;
    StageAcoustics stage_acoustics;
    StageLighting stage_lighting;
    std::optional<VID> controlled_ent_vid;
    std::optional<PlayerId> spectator_target_player_id;
    std::optional<VID> mouse_trailer_vid;
    ContactBookkeeping contact;
    std::vector<EntToolState> ent_tool_states;
    std::vector<WorldPrompt> world_prompts;
    std::vector<DebugRectAnnotation> debug_rect_annotations;
    std::vector<DebugLabelAnnotation> debug_label_annotations;
    Vec2 play_cam_pos;
};

struct SimPlayerSlotSnapshot {
    PlayerId player_id = kInvalidPlayerId;
    std::optional<VID> ent_vid;
    bool connected = false;
    std::string display_name;
    InputFrame input_frame = InputFrame::New();
    InputFrame previous_input_frame = InputFrame::New();
    PlayingInputs inputs = PlayingInputs::New();
    PlayingInputs immediate_inputs = PlayingInputs::New();
};

struct SimNetEntLinkSnapshot {
    network::NetEntId net_id = network::kInvalidNetEntId;
    VID local_vid{};
    bool has_input_owner = false;
    PlayerId input_owner_player_id = kInvalidPlayerId;
};

struct SimNetEntIdAliasSnapshot {
    network::NetEntId from_id = network::kInvalidNetEntId;
    network::NetEntId to_id = network::kInvalidNetEntId;
};

struct SimSnapshot {
    Mode mode = Mode::Title;
    Settings settings;
    PlayingInputs playing_inputs;
    PlayingInputs immediate_playing_inputs;
    PlayingInputSnapshot playing_input_snapshot;
    PlayingInputSnapshot previous_playing_input_snapshot;
    PlayingInputSnapshot previous_immediate_playing_input_snapshot;
    StageRotationState stage_rotation;
    PlayerTuningState player_tuning;
    bool running = true;
    std::uint32_t scene_frame = 0;
    std::uint32_t frame = 0;
    std::uint32_t stage_frame = 0;
    DetRng drng;
    DetRng stagegen_drng;
    Mode menu_return_to = Mode::Title;
    bool game_over = false;
    bool pause = false;
    bool win = false;
    StageLoadTarget respawn_target = StageLoadTarget::ForQuestStage("classic", "classic_mines_1");
    std::optional<StageTransitionTarget> pending_stage_transition;
    MultiplayerRespawnMode multiplayer_respawn_mode = MultiplayerRespawnMode::GenerousNextLevel;
    std::uint32_t points = 0;
    std::uint32_t deaths = 0;
    std::uint32_t depth = 0;
    std::int32_t sac_altar_favor = 0;
    std::uint32_t sac_altar_reward_tier = 0;
    std::vector<VID> interact_claimed_vids_this_frame;
    QuestState quest_state;
    std::vector<SimPlayerSlotSnapshot> players;
    std::uint32_t frame_pause = 0;
    DebugLevelConfig debug_level;
    EntPool ents;
    std::vector<VID> area_listener_vids;
    Stage stage;
    ContactBookkeeping contact;
    std::vector<EntToolState> ent_tool_states;
    network::NetEntId net_next_local_ent_id = 1;
    std::vector<SimNetEntLinkSnapshot> net_ent_links;
    std::vector<SimNetEntIdAliasSnapshot> net_ent_id_aliases;
};

struct LocalOverlaySnapshot {
    std::vector<PlayerId> local_player_ids;
    PlayerId primary_local_player_id = kInvalidPlayerId;
    std::optional<PlayerId> spectator_target_player_id;
    std::vector<DebugLocalPlayerBot> debug_local_player_bots;
    DebugInputOverrideState debug_input_override;
};

GameplaySnapshot MakeGameplaySnapshot(const State& state, const Graphics& graphics);
void RestoreGameplaySnapshot(const GameplaySnapshot& snapshot, State& state, Graphics& graphics);
std::vector<std::uint8_t> SerializeGameplaySnapshotToBytes(const GameplaySnapshot& snapshot);
bool DeserializeGameplaySnapshotFromBytes(
    const std::vector<std::uint8_t>& bytes,
    GameplaySnapshot& snapshot
);

SimSnapshot MakeSimSnapshot(const State& state);
void RestoreSimSnapshot(const SimSnapshot& snapshot, State& state, Graphics& graphics);
std::vector<std::uint8_t> SerializeSimSnapshotToBytes(const SimSnapshot& snapshot);
bool DeserializeSimSnapshotFromBytes(
    const std::vector<std::uint8_t>& bytes,
    SimSnapshot& snapshot
);

LocalOverlaySnapshot CaptureLocalOverlaySnapshot(const State& state);
void RestoreLocalOverlaySnapshot(
    const LocalOverlaySnapshot& snapshot,
    State& state,
    const Graphics& graphics
);

} // namespace splonks

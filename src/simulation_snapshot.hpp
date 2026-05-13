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
    std::optional<std::size_t> video_settings_target_window_size_index;
    std::optional<std::size_t> video_settings_target_resolution_index;
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

GameplaySnapshot MakeGameplaySnapshot(const State& state, const Graphics& graphics);
void RestoreGameplaySnapshot(const GameplaySnapshot& snapshot, State& state, Graphics& graphics);

} // namespace splonks

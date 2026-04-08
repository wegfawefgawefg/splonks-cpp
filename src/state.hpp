#pragma once

#include "entity_manager.hpp"
#include "inputs.hpp"
#include "menu_settings.hpp"
#include "menu_title.hpp"
#include "menu_ui.hpp"
#include "menu_video.hpp"
#include "settings.hpp"
#include "sid.hpp"
#include "special_effects/special_effect.hpp"
#include "stage.hpp"

#include <cstdint>
#include <array>
#include <memory>
#include <optional>
#include <vector>

namespace splonks {

enum class Mode {
    Title,
    Settings,
    VideoSettings,
    UiSettings,
    Playing,
    StageTransition,
    GameOver,
    Win,
};

constexpr std::uint32_t kStageSettleFrames = 100;

enum class ContactInteractionKind {
    Harm,
};

enum class DebugLevelKind {
    Test1,
    HangTest,
};

struct HangTestLevelConfig {
    int stage_height_tiles = 128;
    int cutout_drop_tiles = 8;
    int cutout_height_tiles = 2;
};

struct DebugLevelConfig {
    DebugLevelKind kind = DebugLevelKind::Test1;
    bool player_has_gloves = false;
    HangTestLevelConfig hang_test;
};

struct ContactCooldownEntry {
    VID source_vid;
    VID target_vid;
    ContactInteractionKind kind = ContactInteractionKind::Harm;
    std::uint32_t expires_on_stage_frame = 0;
};

enum class ToolKind : std::uint8_t {
    ThrowPot,
    ThrowBomb,
    ThrowRope,
};

constexpr std::size_t kToolSlotCount = 2;

struct ToolSlot {
    ToolKind kind = ToolKind::ThrowPot;
    std::uint16_t count = 0;
    std::uint16_t cooldown = 0;
    bool active = false;
};

struct EntityToolState {
    VID owner_vid;
    std::array<ToolSlot, kToolSlotCount> slots{};
};

struct State {
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
    std::optional<std::size_t> video_settings_target_window_size_index;
    std::optional<std::size_t> video_settings_target_resolution_index;
    std::optional<bool> video_settings_target_fullscreen;
    bool rebuild_render_texture = false;
    bool choosing_control_binding = false;
    bool show_entity_collision_boxes = false;
    bool running = true;
    double now = 0.0;
    float time_since_last_update = 0.0F;
    std::uint32_t scene_frame = 0;
    std::uint32_t frame = 0;
    std::uint32_t stage_frame = 0;
    Mode menu_return_to = Mode::Title;
    bool game_over = false;
    bool pause = false;
    bool win = false;
    std::optional<StageType> next_stage = StageType::Test1;
    std::uint32_t points = 0;
    std::uint32_t deaths = 0;
    std::uint32_t frame_pause = 0;
    DebugLevelConfig debug_level;
    EntityManager entity_manager;
    std::vector<std::unique_ptr<SpecialEffect>> special_effects;
    SID sid;
    Stage stage;
    std::optional<VID> player_vid;
    std::optional<VID> controlled_entity_vid;
    std::optional<VID> mouse_trailer_vid;
    std::vector<ContactCooldownEntry> contact_cooldowns;
    std::vector<EntityToolState> entity_tool_states;

    static State New();
    void SetMode(Mode new_mode);
    void RebuildSid();
    void StepContactCooldowns();
    bool HasContactCooldown(
        const VID& source_vid,
        const VID& target_vid,
        ContactInteractionKind kind
    ) const;
    void AddContactCooldown(
        const VID& source_vid,
        const VID& target_vid,
        ContactInteractionKind kind,
        std::uint32_t duration
    );
    void StepEntityToolStates();
    EntityToolState* FindEntityToolStateMut(const VID& owner_vid);
    const EntityToolState* FindEntityToolState(const VID& owner_vid) const;
    ToolSlot* FindToolSlotMut(const VID& owner_vid, std::size_t slot_index);
    const ToolSlot* FindToolSlot(const VID& owner_vid, std::size_t slot_index) const;
    ToolSlot& EnsureToolSlot(const VID& owner_vid, std::size_t slot_index);
};

bool IsStageWon(const State& state);
void StepSpecialEffects(State& state);

} // namespace splonks

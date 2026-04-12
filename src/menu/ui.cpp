#include "menu/ui.hpp"

#include "audio.hpp"
#include "graphics.hpp"
#include "settings.hpp"
#include "state.hpp"

#include <algorithm>

namespace splonks {

namespace {

constexpr float kUiScaleStep = 0.05F;

void AdjustUiScale(float& value, float delta, Audio& audio) {
    const float adjusted = std::clamp(value + delta, 0.25F, 1.50F);
    if (adjusted == value) {
        PlayMenuSoundCant(audio);
        return;
    }

    value = adjusted;
    PlayMenuSoundCursorMove(audio);
}

} // namespace

const char* GetUiSettingsMenuOptionName(UiSettingsMenuOption option) {
    switch (option) {
    case UiSettingsMenuOption::IconScale:
        return "Icon Scale";
    case UiSettingsMenuOption::StatusIconScale:
        return "Status Icon Scale";
    case UiSettingsMenuOption::ToolSlotScale:
        return "Tool Slot Scale";
    case UiSettingsMenuOption::ToolIconScale:
        return "Tool Icon Scale";
    case UiSettingsMenuOption::Back:
        return "Back";
    }

    return "";
}

void ProcessInputUiSettingsMenu(
    SDL_Window* window,
    State& state,
    Audio& audio,
    Graphics& graphics,
    float dt
) {
    (void)window;
    (void)graphics;
    (void)dt;

    const bool confirm_pressed = state.menu_inputs.confirm.pressed;
    const bool up_pressed = state.menu_inputs.up.pressed;
    const bool down_pressed = state.menu_inputs.down.pressed;
    const bool left_pressed = state.menu_inputs.left.pressed;
    const bool right_pressed = state.menu_inputs.right.pressed;

    auto adjust_selected = [&](float& value) {
        if (left_pressed) {
            AdjustUiScale(value, -kUiScaleStep, audio);
            SaveSettings(state.settings);
        } else if (right_pressed) {
            AdjustUiScale(value, kUiScaleStep, audio);
            SaveSettings(state.settings);
        }
    };

    switch (state.ui_settings_menu_selection) {
    case UiSettingsMenuOption::IconScale:
        if (up_pressed) {
            PlayMenuSoundCant(audio);
        } else if (down_pressed) {
            state.ui_settings_menu_selection = UiSettingsMenuOption::StatusIconScale;
            PlayMenuSoundCursorMove(audio);
        } else {
            adjust_selected(state.settings.ui.icon_scale);
        }
        break;
    case UiSettingsMenuOption::StatusIconScale:
        if (up_pressed) {
            state.ui_settings_menu_selection = UiSettingsMenuOption::IconScale;
            PlayMenuSoundCursorMove(audio);
        } else if (down_pressed) {
            state.ui_settings_menu_selection = UiSettingsMenuOption::ToolSlotScale;
            PlayMenuSoundCursorMove(audio);
        } else {
            adjust_selected(state.settings.ui.status_icon_scale);
        }
        break;
    case UiSettingsMenuOption::ToolSlotScale:
        if (up_pressed) {
            state.ui_settings_menu_selection = UiSettingsMenuOption::StatusIconScale;
            PlayMenuSoundCursorMove(audio);
        } else if (down_pressed) {
            state.ui_settings_menu_selection = UiSettingsMenuOption::ToolIconScale;
            PlayMenuSoundCursorMove(audio);
        } else {
            adjust_selected(state.settings.ui.tool_slot_scale);
        }
        break;
    case UiSettingsMenuOption::ToolIconScale:
        if (up_pressed) {
            state.ui_settings_menu_selection = UiSettingsMenuOption::ToolSlotScale;
            PlayMenuSoundCursorMove(audio);
        } else if (down_pressed) {
            state.ui_settings_menu_selection = UiSettingsMenuOption::Back;
            PlayMenuSoundCursorMove(audio);
        } else {
            adjust_selected(state.settings.ui.tool_icon_scale);
        }
        break;
    case UiSettingsMenuOption::Back:
        if (confirm_pressed) {
            state.SetMode(Mode::Settings);
            PlayMenuSoundCant(audio);
        } else if (up_pressed) {
            state.ui_settings_menu_selection = UiSettingsMenuOption::ToolIconScale;
            PlayMenuSoundCursorMove(audio);
        } else if (down_pressed) {
            PlayMenuSoundCant(audio);
        }
        break;
    }
}

} // namespace splonks

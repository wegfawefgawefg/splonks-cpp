#include "debug_playback_internal.hpp"

#include "settings.hpp"

namespace splonks::debug_playback_internal {

bool SyncDebugUiSettings(DebugPlayback& debug, State& state) {
    bool changed = false;

    if (state.settings.debug_ui.menu_visible != debug.ui_visible) {
        state.settings.debug_ui.menu_visible = debug.ui_visible;
        changed = true;
    }
    if (state.settings.debug_ui.playback_visible != debug.playback_window_visible) {
        state.settings.debug_ui.playback_visible = debug.playback_window_visible;
        changed = true;
    }
    if (state.settings.debug_ui.level_visible != debug.level_window_visible) {
        state.settings.debug_ui.level_visible = debug.level_window_visible;
        changed = true;
    }
    if (state.settings.debug_ui.entities_visible != debug.entity_inspector_visible) {
        state.settings.debug_ui.entities_visible = debug.entity_inspector_visible;
        changed = true;
    }
    if (state.settings.debug_ui.entity_annotations_visible != debug.entity_annotations_visible) {
        state.settings.debug_ui.entity_annotations_visible = debug.entity_annotations_visible;
        changed = true;
    }
    if (state.settings.debug_ui.ui_settings_visible != debug.ui_settings_window_visible) {
        state.settings.debug_ui.ui_settings_visible = debug.ui_settings_window_visible;
        changed = true;
    }
    if (state.settings.debug_ui.post_fx_settings_visible != debug.post_fx_settings_window_visible) {
        state.settings.debug_ui.post_fx_settings_visible = debug.post_fx_settings_window_visible;
        changed = true;
    }
    if (state.settings.debug_ui.lighting_settings_visible != debug.lighting_settings_window_visible) {
        state.settings.debug_ui.lighting_settings_visible = debug.lighting_settings_window_visible;
        changed = true;
    }
    if (state.settings.debug_ui.graphics_settings_visible != debug.graphics_settings_window_visible) {
        state.settings.debug_ui.graphics_settings_visible = debug.graphics_settings_window_visible;
        changed = true;
    }

    if (changed) {
        SaveSettings(state.settings);
    }

    return changed;
}

} // namespace splonks::debug_playback_internal

#include "debug/playback_internal.hpp"

#include "imgui_layer.hpp"
#include "inputs.hpp"
#include "step.hpp"
#include "render/terrain_lighting.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstring>

namespace splonks::debug_playback_internal {

void ClampPlaybackIndex(DebugPlayback& debug) {
    if (debug.recorded_snapshots.empty()) {
        debug.playback_index = 0;
        return;
    }

    if (debug.playback_index >= debug.recorded_snapshots.size()) {
        debug.playback_index = debug.recorded_snapshots.size() - 1;
    }
}

void PushSnapshot(DebugPlayback& debug, const State& state, const Graphics& graphics) {
    if (!debug.recording) {
        return;
    }

    debug.recorded_snapshots.push_back(MakeGameplaySnapshot(state, graphics));
    while (static_cast<int>(debug.recorded_snapshots.size()) > debug.max_snapshots) {
        debug.recorded_snapshots.pop_front();
        if (debug.playback_index > 0) {
            debug.playback_index -= 1;
        }
    }
    ClampPlaybackIndex(debug);
}

void StartRecording(DebugPlayback& debug, const State& state, const Graphics& graphics) {
    debug.recorded_snapshots.clear();
    debug.playback_index = 0;
    debug.recording = true;
    PushSnapshot(debug, state, graphics);
}

void StopRecording(DebugPlayback& debug) {
    debug.recording = false;
    ClampPlaybackIndex(debug);
}

void EnterPlayback(DebugPlayback& debug, const State& state, const Graphics& graphics) {
    if (debug.recorded_snapshots.empty()) {
        return;
    }

    debug.recording = false;
    debug.live_resume_snapshot = MakeGameplaySnapshot(state, graphics);
    debug.playback_active = true;
    debug.pause_live_simulation = true;
    debug.playback_index = debug.recorded_snapshots.size() - 1;
    ClampPlaybackIndex(debug);
}

void ExitPlayback(DebugPlayback& debug, State& state, Graphics& graphics) {
    debug.playback_active = false;
    debug.skip_live_simulation_once = true;
    if (debug.live_resume_snapshot.has_value()) {
        RestoreGameplaySnapshot(*debug.live_resume_snapshot, state, graphics);
        debug.live_resume_snapshot.reset();
    }
    InvalidateTerrainLightingCache(state);
}

namespace {

bool ShouldProcessGameplayInput(const DebugPlayback& debug) {
    if (debug.playback_active) {
        return false;
    }

    if (ImGuiWantsMouse()) {
        return false;
    }

    return true;
}

} // namespace

void AdvanceLiveSimulation(
    SDL_Window* window,
    SDL_Renderer* renderer,
    State& state,
    Audio& audio,
    Graphics& graphics,
    DebugPlayback& debug,
    float frame_dt
) {
    graphics.debug_lock_play_camera = false;

    if (debug.skip_live_simulation_once) {
        debug.skip_live_simulation_once = false;
        return;
    }

    if (ShouldProcessGameplayInput(debug)) {
        ProcessInput(window, renderer, state, audio, graphics, frame_dt);
    }

    if (debug.pause_live_simulation) {
        if (!debug.step_live_simulation_once) {
            return;
        }

        debug.step_live_simulation_once = false;
        StepSingleTick(state, audio, graphics);
        PushSnapshot(debug, state, graphics);
        return;
    }

    const float scaled_dt = frame_dt * debug.time_scale;
    state.time_since_last_update += scaled_dt;
    while (state.time_since_last_update > kTimestep) {
        state.time_since_last_update -= kTimestep;
        StepSingleTick(state, audio, graphics);
        PushSnapshot(debug, state, graphics);
    }
}

} // namespace splonks::debug_playback_internal

namespace splonks {

DebugPlayback DebugPlayback::New() {
    DebugPlayback result;
    const char* default_path = "debug_recording.splrec";
    std::strncpy(result.file_path.data(), default_path, result.file_path.size() - 1);
    result.file_path[result.file_path.size() - 1] = '\0';
    return result;
}

void DrawDebugPlaybackControls(
    DebugPlayback& debug,
    State& state,
    Graphics& graphics,
    SDL_Window* window,
    SDL_Renderer* renderer
) {
    if (ImGui::IsKeyPressed(ImGuiKey_F1)) {
        debug.imgui_visible = !debug.imgui_visible;
    }

    if (!debug.imgui_visible) {
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_F2)) {
        debug.ui_visible = !debug.ui_visible;
    }

    debug_playback_internal::DrawDebugMenu(debug, state);
    debug_playback_internal::DrawSimulationControls(debug, state, graphics);
    debug_playback_internal::DrawLevelControls(debug, state, graphics);
    debug_playback_internal::DrawBorderControls(debug, state, graphics);
    debug_playback_internal::DrawDebugOverlayWindow(debug, state);
    debug_playback_internal::DrawUiSettingsWindow(debug, state);
    debug_playback_internal::DrawPostFxSettingsWindow(debug, state, graphics);
    debug_playback_internal::DrawLightingSettingsWindow(debug, state, graphics);
    debug_playback_internal::DrawGraphicsSettingsWindow(debug, state, graphics, window, renderer);
}

void DrawDebugPlaybackInspector(DebugPlayback& debug, State& state, const Graphics& graphics) {
    if (!debug.imgui_visible) {
        return;
    }

    debug_playback_internal::DrawEntityInspector(debug, state, graphics);
}

void RunSimulationWithDebugControls(
    SDL_Window* window,
    SDL_Renderer* renderer,
    State& state,
    Audio& audio,
    Graphics& graphics,
    DebugPlayback& debug,
    float frame_dt
) {
    if (debug.playback_active) {
        debug_playback_internal::ClampPlaybackIndex(debug);
        if (!debug.recorded_snapshots.empty()) {
            RestoreGameplaySnapshot(debug.recorded_snapshots[debug.playback_index], state, graphics);
            InvalidateTerrainLightingCache(state);
        }
        graphics.debug_lock_play_camera = true;
        return;
    }

    debug_playback_internal::AdvanceLiveSimulation(
        window,
        renderer,
        state,
        audio,
        graphics,
        debug,
        frame_dt
    );
}

} // namespace splonks

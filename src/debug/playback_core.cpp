#include "debug/playback_internal.hpp"

#include "audio_acoustics.hpp"
#include "audio_emitters.hpp"
#include "imgui_layer.hpp"
#include "inputs.hpp"
#include "step.hpp"
#include "stage_lighting.hpp"
#include "stage_acoustics.hpp"
#include "tile_archetype.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

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
    InvalidateStageLighting(state);
    InvalidateStageAcoustics(state);
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

bool IsShakeBrushActive(const State& state) {
    const DebugShakeBrushState& brush = state.debug_shake_brush;
    return brush.enabled &&
           ((brush.affect_foreground_tiles && brush.foreground_tile_amount > 0.0F) ||
            (brush.affect_background_tiles && brush.background_tile_amount > 0.0F) ||
            (brush.affect_entities && brush.entity_amount > 0.0F));
}

bool IsFluidBrushActive(const State& state) {
    return state.debug_fluid_brush.enabled;
}

Vec2 StageCenterWorld(const Stage& stage) {
    return Vec2::New(
        static_cast<float>(stage.GetWidth()) * 0.5F,
        static_cast<float>(stage.GetHeight()) * 0.5F
    );
}

bool CanFluidBrushOccupyTile(Tile terrain_tile) {
    if (terrain_tile == Tile::Air) {
        return true;
    }
    const TileArchetype& archetype = GetTileArchetype(terrain_tile);
    return !archetype.simulated_fluid && archetype.transparent && !archetype.solid &&
           !archetype.one_way_top_solid;
}

void PushChangedTile(std::vector<IVec2>& changed_tiles, const IVec2& tile_coord) {
    if (std::find(changed_tiles.begin(), changed_tiles.end(), tile_coord) != changed_tiles.end()) {
        return;
    }
    changed_tiles.push_back(tile_coord);
}

void ApplyShakeBrush(State& state, Graphics& graphics) {
    if (state.mode != Mode::Playing || !IsShakeBrushActive(state) || ImGuiWantsMouse()) {
        return;
    }

    float mouse_x = 0.0F;
    float mouse_y = 0.0F;
    const SDL_MouseButtonFlags mouse_buttons = SDL_GetMouseState(&mouse_x, &mouse_y);
    if ((mouse_buttons & SDL_BUTTON_LMASK) == 0) {
        return;
    }

    const DebugShakeBrushState& brush = state.debug_shake_brush;
    const UVec2 mouse_pos = state.immediate_playing_inputs.mouse_pos;
    const Vec2 mouse_world = graphics.ScreenToWc(mouse_pos);
    const float radius_tiles = std::max(0.0F, brush.radius_tiles);

    if (brush.affect_foreground_tiles && brush.foreground_tile_amount > 0.0F) {
        AddShake(
            state,
            mouse_world,
            brush.foreground_tile_amount,
            radius_tiles,
            ShakeMask::ForegroundTiles
        );
    }
    if (brush.affect_background_tiles && brush.background_tile_amount > 0.0F) {
        AddShake(
            state,
            mouse_world,
            brush.background_tile_amount,
            radius_tiles,
            ShakeMask::BackgroundTiles
        );
    }
    if (brush.affect_entities && brush.entity_amount > 0.0F) {
        AddShake(state, mouse_world, brush.entity_amount, radius_tiles, ShakeMask::Entities);
    }
}

void ApplyFluidBrush(State& state, Graphics& graphics) {
    if (state.mode != Mode::Playing || !IsFluidBrushActive(state) || ImGuiWantsMouse()) {
        return;
    }
    if (state.net_session.role == network::NetRole::Peer) {
        return;
    }

    float mouse_x = 0.0F;
    float mouse_y = 0.0F;
    const SDL_MouseButtonFlags mouse_buttons = SDL_GetMouseState(&mouse_x, &mouse_y);
    const bool paint_water = (mouse_buttons & SDL_BUTTON_LMASK) != 0;
    const bool erase_fluid = (mouse_buttons & SDL_BUTTON_RMASK) != 0;
    if (!paint_water && !erase_fluid) {
        return;
    }

    DebugFluidBrushState& brush = state.debug_fluid_brush;
    if (brush.mode == DebugFluidBrushState::Mode::GlobalGravityDirection) {
        if (paint_water) {
            constexpr float kGravityPickerScalePx = 64.0F;
            const Vec2 mouse_world = graphics.ScreenToWc(state.immediate_playing_inputs.mouse_pos);
            const Vec2 gravity = (mouse_world - StageCenterWorld(state.stage)) /
                                 kGravityPickerScalePx;
            state.settings.fluid.gravity_x = std::clamp(gravity.x, -4.0F, 4.0F);
            state.settings.fluid.gravity_y = std::clamp(gravity.y, -4.0F, 4.0F);
        }
        return;
    }

    const IVec2 mouse_tile = graphics.ScreenToTileCoords(state.immediate_playing_inputs.mouse_pos);
    const int radius_tiles = std::max(0, brush.radius_tiles);
    const float radius_for_falloff = std::max(1.0F, static_cast<float>(radius_tiles));
    std::vector<IVec2> changed_tiles;

    for (int y = mouse_tile.y - radius_tiles; y <= mouse_tile.y + radius_tiles; ++y) {
        for (int x = mouse_tile.x - radius_tiles; x <= mouse_tile.x + radius_tiles; ++x) {
            const float dx = static_cast<float>(x - mouse_tile.x);
            const float dy = static_cast<float>(y - mouse_tile.y);
            const float distance = std::sqrt((dx * dx) + (dy * dy));
            if (distance > static_cast<float>(radius_tiles)) {
                continue;
            }

            const IVec2 wrapped = state.stage.WrapTileCoord(IVec2::New(x, y));
            if (!state.stage.IsTileCoordInside(wrapped.x, wrapped.y)) {
                continue;
            }

            const Tile current_tile = state.stage.GetTile(
                static_cast<unsigned int>(wrapped.x),
                static_cast<unsigned int>(wrapped.y)
            );
            const float falloff = std::clamp(1.0F - (distance / (radius_for_falloff + 1.0F)), 0.0F, 1.0F);
            if (brush.mode == DebugFluidBrushState::Mode::PermanentGravity) {
                if (paint_water) {
                    state.stage.SetFluidGravityOverride(
                        wrapped,
                        Vec2::New(brush.paint_gravity_x, brush.paint_gravity_y)
                    );
                } else if (erase_fluid) {
                    state.stage.ClearFluidGravityOverride(wrapped);
                }
                continue;
            }
            if (brush.mode == DebugFluidBrushState::Mode::TemporaryGravity) {
                if (paint_water) {
                    state.stage.AddFluidTempGravity(
                        wrapped,
                        Vec2::New(brush.paint_gravity_x, brush.paint_gravity_y) * falloff
                    );
                } else if (erase_fluid) {
                    state.stage.ClearFluidTempGravity(wrapped);
                }
                continue;
            }

            if (paint_water) {
                if (!brush.replace_solid_tiles && !CanFluidBrushOccupyTile(current_tile)) {
                    continue;
                }
                if (brush.replace_solid_tiles && !CanFluidBrushOccupyTile(current_tile)) {
                    state.stage.SetTile(wrapped, Tile::Air);
                }
                state.stage.SetFluidTile(wrapped, Tile::WaterSwim);
                PushChangedTile(changed_tiles, wrapped);
            } else if (erase_fluid) {
                const Tile fluid_tile = state.stage.GetFluidTile(
                    static_cast<unsigned int>(wrapped.x),
                    static_cast<unsigned int>(wrapped.y)
                );
                if (GetTileArchetype(current_tile).simulated_fluid) {
                    state.stage.SetTile(wrapped, Tile::Air);
                    PushChangedTile(changed_tiles, wrapped);
                }
                if (GetTileArchetype(fluid_tile).simulated_fluid) {
                    state.stage.SetFluidTile(wrapped, Tile::Air);
                    PushChangedTile(changed_tiles, wrapped);
                }
            }
        }
    }

    if (!changed_tiles.empty()) {
        UpdateStageLightingForTileChanges(state, changed_tiles);
        UpdateStageAcousticsForTileChanges(state, changed_tiles);
    }
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

    ApplyShakeBrush(state, graphics);
    ApplyFluidBrush(state, graphics);

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
    const char* default_host = "127.0.0.1";
    std::strncpy(result.network_join_host.data(), default_host, result.network_join_host.size() - 1);
    result.network_join_host[result.network_join_host.size() - 1] = '\0';
    return result;
}

void DrawDebugPlaybackControls(
    DebugPlayback& debug,
    State& state,
    Audio& audio,
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
    debug_playback_internal::DrawSimulationControls(debug, state, audio, graphics);
    debug_playback_internal::DrawLevelControls(debug, state, graphics);
    debug_playback_internal::DrawBorderControls(debug, state, graphics);
    debug_playback_internal::DrawDebugOverlayWindow(debug, state, graphics);
    debug_playback_internal::DrawShakeBrushWindow(debug, state, graphics);
    debug_playback_internal::DrawAudioBrushWindow(debug, state, audio, graphics);
    debug_playback_internal::DrawFluidBrushWindow(debug, state, graphics);
    debug_playback_internal::DrawAudioSettingsWindow(debug, state);
    debug_playback_internal::DrawUiSettingsWindow(debug, state);
    debug_playback_internal::DrawCameraSettingsWindow(debug, state, graphics);
    debug_playback_internal::DrawPerformanceSettingsWindow(debug, state);
    debug_playback_internal::DrawPlayerTuningWindow(debug, state);
    debug_playback_internal::DrawNetworkWindow(debug, state, graphics);
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

namespace {

void StopDebugAudioBrushLoop(DebugPlayback& debug, Audio& audio) {
    if (IsValidAudioInstanceVID(debug.audio_brush_loop_handle)) {
        (void)audio.StopAudioInstance(debug.audio_brush_loop_handle);
    }
    debug.audio_brush_loop_handle = kInvalidAudioInstanceVID;
    debug.audio_brush_loop_audio_asset_id.reset();
}

AudioPlaybackParams MakeDebugAudioBrushPlaybackParams(
    State& state,
    const Graphics& graphics
) {
    const DebugAudioBrushState& brush = state.debug_audio_brush;
    AudioPlaybackParams params;
    params.volume_scale = brush.volume_scale;
    params.positional = true;
    params.loops = -1;

    const PositionalAudioAcoustics acoustics = ComputePositionalAudioAcoustics(
        state,
        graphics.camera.target,
        brush.source_world_pos
    );
    params.world_pos = acoustics.wrapped_source_world_pos;
    params.direct_gain = acoustics.direct_gain;
    params.low_pass_enabled = acoustics.low_pass_enabled;
    params.low_pass_cutoff_hz = acoustics.low_pass_cutoff_hz;
    params.low_pass_wet = acoustics.low_pass_wet;
    params.reverb_enabled = acoustics.reverb_enabled;
    params.reverb_wet = acoustics.reverb_wet;
    params.reverb_feedback = acoustics.reverb_feedback;
    params.reverb_delay_ms = acoustics.reverb_delay_ms;
    params.reverb_low_pass_cutoff_hz = acoustics.reverb_low_pass_cutoff_hz;
    return params;
}

void UpdateDebugAudioBrushInput(State& state, const Graphics& graphics) {
    if (state.mode != Mode::Playing || !state.debug_audio_brush.enabled || ImGuiWantsMouse()) {
        return;
    }

    float mouse_x = 0.0F;
    float mouse_y = 0.0F;
    const SDL_MouseButtonFlags mouse_buttons = SDL_GetMouseState(&mouse_x, &mouse_y);
    if ((mouse_buttons & SDL_BUTTON_LMASK) != 0) {
        state.debug_audio_brush.source_world_pos =
            graphics.ScreenToWc(state.immediate_playing_inputs.mouse_pos);
        state.debug_audio_brush.source_active = true;
    }
    if ((mouse_buttons & SDL_BUTTON_RMASK) != 0) {
        state.debug_audio_brush.source_active = false;
    }
}

} // namespace

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
            InvalidateStageLighting(state);
    InvalidateStageAcoustics(state);
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

void UpdateDebugAudioBrush(
    DebugPlayback& debug,
    State& state,
    Audio& audio,
    const Graphics& graphics
) {
    audio.SetListenerWorldPos(GetAudioListenerWorldPos(state));
    UpdateDebugAudioBrushInput(state, graphics);

    if (debug.playback_active ||
        state.mode != Mode::Playing ||
        !state.debug_audio_brush.enabled ||
        !state.debug_audio_brush.source_active) {
        StopDebugAudioBrushLoop(debug, audio);
        return;
    }

    const DebugAudioBrushState& brush = state.debug_audio_brush;
    if (debug.audio_brush_loop_audio_asset_id.has_value() &&
        *debug.audio_brush_loop_audio_asset_id != brush.audio_asset_id) {
        StopDebugAudioBrushLoop(debug, audio);
    }

    const AudioPlaybackParams params =
        MakeDebugAudioBrushPlaybackParams(state, graphics);
    if (IsValidAudioInstanceVID(debug.audio_brush_loop_handle) &&
        audio.UpdateAudioInstance(debug.audio_brush_loop_handle, params)) {
        debug.audio_brush_loop_audio_asset_id = brush.audio_asset_id;
        return;
    }

    debug.audio_brush_loop_handle =
        audio.PlayAudioAssetInstance(brush.audio_asset_id, params);
    if (IsValidAudioInstanceVID(debug.audio_brush_loop_handle)) {
        debug.audio_brush_loop_audio_asset_id = brush.audio_asset_id;
        return;
    }

    debug.audio_brush_loop_audio_asset_id.reset();
}

} // namespace splonks

#pragma once

#include "debug_playback.hpp"

namespace splonks::debug_playback_internal {

const char* ModeToString(Mode mode);
const char* DebugLevelKindToString(DebugLevelKind kind);
const char* EntityTypeToString(EntityType type);
const char* DisplayStateToString(EntityDisplayState state);
const char* SuperStateToString(EntitySuperState state);
const char* LeftOrRightToString(LeftOrRight facing);

void ClampPlaybackIndex(DebugPlayback& debug);
void PushSnapshot(DebugPlayback& debug, const State& state, const Graphics& graphics);
void StartRecording(DebugPlayback& debug, const State& state, const Graphics& graphics);
void StopRecording(DebugPlayback& debug);
void EnterPlayback(DebugPlayback& debug, const State& state, const Graphics& graphics);
void ExitPlayback(DebugPlayback& debug, State& state, Graphics& graphics);

bool SaveRecordingToFile(const DebugPlayback& debug, std::string* status_out);
bool LoadRecordingFromFile(DebugPlayback& debug, std::string* status_out);
bool ExportRecordingToTextFile(
    const DebugPlayback& debug,
    const Graphics& graphics,
    std::string* status_out
);

void DrawSimulationControls(DebugPlayback& debug, State& state, Graphics& graphics);
void DrawDebugMenu(DebugPlayback& debug, State& state);
void DrawLevelControls(DebugPlayback& debug, State& state, Graphics& graphics);
void DrawEntityInspector(DebugPlayback& debug, State& state, const Graphics& graphics);
void DrawEntityAnnotations(DebugPlayback& debug, State& state);
void DrawUiSettingsWindow(DebugPlayback& debug, State& state);
void DrawPostFxSettingsWindow(DebugPlayback& debug, State& state, const Graphics& graphics);
void DrawLightingSettingsWindow(DebugPlayback& debug, State& state, Graphics& graphics);
void DrawGraphicsSettingsWindow(
    DebugPlayback& debug,
    State& state,
    Graphics& graphics,
    SDL_Window* window,
    SDL_Renderer* renderer
);

void AdvanceLiveSimulation(
    SDL_Window* window,
    SDL_Renderer* renderer,
    State& state,
    Audio& audio,
    Graphics& graphics,
    DebugPlayback& debug,
    float frame_dt
);

} // namespace splonks::debug_playback_internal

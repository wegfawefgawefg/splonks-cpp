#pragma once

#include "graphics.hpp"
#include "settings.hpp"
#include "state.hpp"

#include <SDL3/SDL.h>
#include <gubsy/runtime.hpp>
#include <vector>

namespace splonks::gubsy_shell {

struct LobbyConfig {
    MultiplayerRespawnMode respawn_mode = MultiplayerRespawnMode::GenerousNextLevel;
    std::vector<EntType> character_by_player;
};

struct Shell {
    GubsyRuntime runtime;
    State* state = nullptr;
    LobbyConfig lobby_config;
    MenuCommandId in_game_resume_command = kMenuIdInvalid;
    MenuCommandId in_game_restart_run_command = kMenuIdInvalid;
    MenuCommandId in_game_quit_to_main_menu_command = kMenuIdInvalid;
    bool block_next_menu_input = false;
    bool block_menu_input_until_release = false;
    bool direct_join_pending = false;
    std::string direct_join_endpoint;
    std::uint64_t direct_join_started_ms = 0;
};

bool Init(Shell& shell, State& state, SDL_Window* window, SDL_Renderer* renderer,
          const Graphics& graphics);
bool InitHeadless(Shell& shell, State& state);
bool InitOwned(Shell& shell, State& state, const Settings& settings);
GubsyFrame GetFrame(Shell& shell);
void ProcessEvent(Shell& shell, const SDL_Event& event);
void UpdateDeviceState(Shell& shell);
void ApplyLobbyGameplayInput(Shell& shell);
bool TextEditActive(Shell& shell);
bool DrawFrameToWindow(Shell& shell);
void PresentFrame(Shell& shell);
int ConfiguredFrameCapFps(Shell& shell);
void BeginDebugFrame(Shell& shell, float dt);
bool OpenInGameMenu(Shell& shell);
void CloseInGameMenu(Shell& shell);
bool InGameMenuOpen(Shell& shell);
void UpdateMenu(Shell& shell, const State& state, float dt, int screen_width, int screen_height);
void RenderMenu(Shell& shell, SDL_Renderer* renderer, int screen_width, int screen_height);
void RenderAlerts(Shell& shell, SDL_Renderer* renderer, int screen_width);
void UpdateTitleMenu(Shell& shell, State& state, Graphics& graphics, float dt,
                     int screen_width, int screen_height);
void RenderTitleMenu(Shell& shell, SDL_Renderer* renderer, int screen_width, int screen_height);
void RenderDebug(Shell& shell, SDL_Renderer* renderer, int screen_width, int screen_height);
void ShutdownDebug(Shell& shell);
void AddAlert(Shell& shell, const std::string& text,
              GubsyAlertSeverity severity = GubsyAlertSeverity::Info);
void Shutdown(Shell& shell);

} // namespace splonks::gubsy_shell

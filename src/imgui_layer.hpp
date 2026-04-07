#pragma once

#include <SDL3/SDL.h>

namespace splonks {

bool InitImGuiLayer(SDL_Window* window, SDL_Renderer* renderer);
void ShutdownImGuiLayer();
void ImGuiLayerNewFrame();
void ImGuiLayerRender();
void ImGuiLayerProcessEvent(const SDL_Event& event);
bool ImGuiWantsMouse();
bool ImGuiWantsKeyboard();

} // namespace splonks

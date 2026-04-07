#include "imgui_layer.hpp"

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>

namespace {

bool g_imgui_initialized = false;
SDL_Renderer* g_imgui_renderer = nullptr;

} // namespace

namespace splonks {

bool InitImGuiLayer(SDL_Window* window, SDL_Renderer* renderer) {
    if (g_imgui_initialized) {
        return true;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    if (!ImGui_ImplSDL3_InitForSDLRenderer(window, renderer)) {
        ImGui::DestroyContext();
        return false;
    }

    if (!ImGui_ImplSDLRenderer3_Init(renderer)) {
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        return false;
    }

    g_imgui_initialized = true;
    g_imgui_renderer = renderer;
    return true;
}

void ShutdownImGuiLayer() {
    if (!g_imgui_initialized) {
        return;
    }

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    g_imgui_initialized = false;
    g_imgui_renderer = nullptr;
}

void ImGuiLayerNewFrame() {
    if (!g_imgui_initialized) {
        return;
    }

    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayerRender() {
    if (!g_imgui_initialized || g_imgui_renderer == nullptr) {
        return;
    }

    ImGui::Render();
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), g_imgui_renderer);
}

void ImGuiLayerProcessEvent(const SDL_Event& event) {
    if (!g_imgui_initialized) {
        return;
    }

    switch (event.type) {
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
    case SDL_EVENT_GAMEPAD_ADDED:
    case SDL_EVENT_GAMEPAD_REMOVED:
    case SDL_EVENT_GAMEPAD_REMAPPED:
    case SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN:
    case SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION:
    case SDL_EVENT_GAMEPAD_TOUCHPAD_UP:
    case SDL_EVENT_GAMEPAD_SENSOR_UPDATE:
        return;
    default:
        break;
    }

    ImGui_ImplSDL3_ProcessEvent(&event);
}

bool ImGuiWantsMouse() {
    if (!g_imgui_initialized) {
        return false;
    }

    return ImGui::GetIO().WantCaptureMouse;
}

bool ImGuiWantsKeyboard() {
    if (!g_imgui_initialized) {
        return false;
    }

    return ImGui::GetIO().WantCaptureKeyboard;
}

} // namespace splonks

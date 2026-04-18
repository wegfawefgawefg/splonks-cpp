#include "audio.hpp"
#include "cli.hpp"
#include "debug/playback.hpp"
#include "entity/archetype.hpp"
#include "graphics.hpp"
#include "imgui_layer.hpp"
#include "inputs.hpp"
#include "render/render.hpp"
#include "render/postfx.hpp"
#include "state.hpp"
#include "step.hpp"
#include "stage_lighting.hpp"
#include "tools/tool_archetype.hpp"
#include "text.hpp"

#include <SDL3/SDL.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

constexpr int kWindowWidth = 1920;
constexpr int kWindowHeight = 1080;

[[noreturn]] void ThrowSdlError(const char* message) {
    throw std::runtime_error(std::string(message) + ": " + SDL_GetError());
}

splonks::UVec2 GetWindowDims(SDL_Window* window) {
    int window_width = 0;
    int window_height = 0;
    SDL_GetWindowSize(window, &window_width, &window_height);
    return splonks::UVec2::New(
        static_cast<unsigned int>(std::max(window_width, 1)),
        static_cast<unsigned int>(std::max(window_height, 1))
    );
}

void RebaseCwdToRepoRoot() {
    if (const char* base_path = SDL_GetBasePath()) {
        std::filesystem::path probe = std::filesystem::path(base_path);
        for (int i = 0; i < 4; ++i) {
            if (std::filesystem::exists(probe / "assets") &&
                std::filesystem::exists(probe / "src") &&
                std::filesystem::exists(probe / "CMakeLists.txt")) {
                std::filesystem::current_path(probe);
                return;
            }
            if (!probe.has_parent_path()) {
                break;
            }
            probe = probe.parent_path();
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    RebaseCwdToRepoRoot();

    if (splonks::RunCliCommand(argc, argv)) {
        return 0;
    }

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* render_texture = nullptr;
    splonks::Graphics graphics;
    splonks::RenderPostFx post_fx = splonks::RenderPostFx::New();
    splonks::Audio audio;
    splonks::DebugPlayback debug = splonks::DebugPlayback::New();

    try {
        ////////////////        GRAPHICS INIT        ////////////////
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
            ThrowSdlError("SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD) failed");
        }
        if (!splonks::InitTextSubsystem()) {
            ThrowSdlError("TTF_Init failed");
        }

        const splonks::Settings loaded_settings = splonks::LoadSettings();

        const SDL_WindowFlags window_flags =
            (loaded_settings.video.fullscreen ? SDL_WINDOW_FULLSCREEN : 0) |
            SDL_WINDOW_HIGH_PIXEL_DENSITY | (!loaded_settings.video.fullscreen ? SDL_WINDOW_RESIZABLE : 0);
        const int startup_width = static_cast<int>(loaded_settings.video.resolution.x);
        const int startup_height = static_cast<int>(loaded_settings.video.resolution.y);
        window = SDL_CreateWindow(
            "Splonks",
            startup_width > 0 ? startup_width : kWindowWidth,
            startup_height > 0 ? startup_height : kWindowHeight,
            window_flags
        );
        if (window == nullptr) {
            ThrowSdlError("SDL_CreateWindow failed");
        }

        renderer = SDL_CreateRenderer(window, SDL_GPU_RENDERER);
        if (renderer == nullptr) {
            renderer = SDL_CreateRenderer(window, nullptr);
        }
        if (renderer == nullptr) {
            ThrowSdlError("SDL_CreateRenderer failed");
        }
        if (!SDL_SetRenderVSync(renderer, loaded_settings.video.vsync ? 1 : 0)) {
            ThrowSdlError("SDL_SetRenderVSync failed");
        }

        if (!splonks::InitImGuiLayer(window, renderer)) {
            ThrowSdlError("InitImGuiLayer failed");
        }

        {
            const std::string sprite_assets_folder = "assets/graphics/aseprite";
            graphics = splonks::Graphics::New(renderer, sprite_assets_folder);
        }
        graphics.dims = loaded_settings.video.resolution;
        graphics.fullscreen = loaded_settings.video.fullscreen;
        graphics.window_dims = GetWindowDims(window);

        const auto rebuild_render_texture = [&]() {
            if (render_texture != nullptr) {
                SDL_DestroyTexture(render_texture);
                render_texture = nullptr;
            }

            render_texture = SDL_CreateTexture(
                renderer,
                SDL_PIXELFORMAT_RGBA8888,
                SDL_TEXTUREACCESS_TARGET,
                static_cast<int>(graphics.dims.x),
                static_cast<int>(graphics.dims.y)
            );
            if (render_texture == nullptr) {
                ThrowSdlError("SDL_CreateTexture render target failed");
            }

            SDL_SetTextureBlendMode(render_texture, SDL_BLENDMODE_BLEND);
            SDL_SetTextureScaleMode(render_texture, SDL_SCALEMODE_NEAREST);
            graphics.camera.offset = splonks::ToVec2(graphics.dims / 2U);
            splonks::RefreshRenderPostFx(post_fx, render_texture, loaded_settings.post_process);
        };

        rebuild_render_texture();
        splonks::InitRenderPostFx(post_fx, renderer, render_texture, loaded_settings.post_process);
        graphics.gpu_renderer_active = post_fx.gpu_renderer_active;

        ////////////////        AUDIO INIT        ////////////////
        try {
            const auto songs = splonks::LoadSongs();
            const auto sounds = splonks::LoadSounds();
            audio = splonks::Audio::New(songs, sounds);
        } catch (const std::exception&) {
            audio = splonks::Audio{};
        }

        ////////////////        MAIN LOOP        ////////////////
        splonks::PopulateEntityArchetypesTable();
        splonks::PopulateToolArchetypesTable();
        splonks::State state = splonks::State::New();
        state.running = true;
        debug.ui_visible = state.settings.debug_ui.menu_visible;
        debug.playback_window_visible = state.settings.debug_ui.playback_visible;
        debug.level_window_visible = state.settings.debug_ui.level_visible;
        debug.entity_inspector_visible = state.settings.debug_ui.entities_visible;
        debug.entity_annotations_visible = state.settings.debug_ui.entity_annotations_visible;
        debug.shake_brush_window_visible = state.settings.debug_ui.shake_brush_visible;
        debug.ui_settings_window_visible = state.settings.debug_ui.ui_settings_visible;
        debug.post_fx_settings_window_visible = state.settings.debug_ui.post_fx_settings_visible;
        debug.lighting_settings_window_visible = state.settings.debug_ui.lighting_settings_visible;
        debug.graphics_settings_window_visible = state.settings.debug_ui.graphics_settings_visible;
        debug.camera_settings_window_visible = state.settings.debug_ui.camera_settings_visible;
        splonks::RefreshRenderPostFx(post_fx, render_texture, state.settings.post_process);
        splonks::RebuildStageLighting(state);

        std::uint64_t last_ticks = SDL_GetTicks();

        while (state.running) {
            SDL_Event event{};
            while (SDL_PollEvent(&event)) {
                splonks::ImGuiLayerProcessEvent(event);
                if (event.type == SDL_EVENT_QUIT) {
                    state.running = false;
                } else if (event.type == SDL_EVENT_WINDOW_RESIZED ||
                           event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
                    graphics.window_dims = GetWindowDims(window);
                } else if (event.type == SDL_EVENT_KEY_DOWN &&
                           event.key.scancode == SDL_SCANCODE_ESCAPE &&
                           !event.key.repeat) {
                    state.running = false;
                }
            }

            const std::uint64_t now = SDL_GetTicks();
            const float dt = static_cast<float>(now - last_ticks) / 1000.0F;
            last_ticks = now;

            if (state.rebuild_render_texture) {
                rebuild_render_texture();
                state.rebuild_render_texture = false;
            }

            splonks::ImGuiLayerNewFrame();
            splonks::DrawDebugPlaybackControls(debug, state, graphics, window, renderer);
            splonks::RunSimulationWithDebugControls(
                window,
                renderer,
                state,
                audio,
                graphics,
                debug,
                dt
            );
            splonks::DrawDebugPlaybackInspector(debug, state, graphics);
            splonks::RefreshRenderPostFx(post_fx, render_texture, state.settings.post_process);
            splonks::Render(renderer, render_texture, post_fx, state, graphics);
            splonks::ImGuiLayerRender();
            SDL_RenderPresent(renderer);
            audio.UpdateCurrentSongStreamData();
        }

        if (render_texture != nullptr) {
            SDL_DestroyTexture(render_texture);
            render_texture = nullptr;
        }
        post_fx.Shutdown();
        if (renderer != nullptr) {
            SDL_DestroyRenderer(renderer);
            renderer = nullptr;
        }
        if (window != nullptr) {
            SDL_DestroyWindow(window);
            window = nullptr;
        }
        graphics.ShutdownTextures();
        graphics.ShutdownText();
        audio.Shutdown();
        splonks::ShutdownImGuiLayer();
        splonks::ShutdownTextSubsystem();
        SDL_Quit();
        return 0;
    } catch (const std::exception& exception) {
        const std::string error_message = exception.what();
        graphics.ShutdownTextures();
        graphics.ShutdownText();
        splonks::ShutdownImGuiLayer();
        if (render_texture != nullptr) {
            SDL_DestroyTexture(render_texture);
        }
        post_fx.Shutdown();
        if (renderer != nullptr) {
            SDL_DestroyRenderer(renderer);
        }
        if (window != nullptr) {
            SDL_DestroyWindow(window);
        }
        audio.Shutdown();
        splonks::ShutdownTextSubsystem();
        SDL_Quit();
        std::cerr << "\n=== Splonks startup failed ===\n";
        std::cerr << error_message << '\n';
        std::cerr << "If this is frame-data or tile-source data, run:\n";
        std::cerr << "  ./build-debug/splonks-cpp --check-frame-data\n";
        std::cerr << "  ./build-debug/splonks-cpp --check-tile-source-data\n\n";
        return 1;
    }
}

#include "audio.hpp"
#include "graphics.hpp"
#include "inputs.hpp"
#include "render.hpp"
#include "state.hpp"
#include "step.hpp"
#include "text.hpp"

#include <SDL3/SDL.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr const char* kX11DialogWindowType = "_NET_WM_WINDOW_TYPE_DIALOG";

[[noreturn]] void ThrowSdlError(const char* message) {
    throw std::runtime_error(std::string(message) + ": " + SDL_GetError());
}

void CenterWindowOnPrimaryDisplay(SDL_Window* window) {
    const SDL_DisplayID primary_display = SDL_GetPrimaryDisplay();
    if (primary_display == 0) {
        return;
    }

    SDL_Rect bounds{};
    if (!SDL_GetDisplayBounds(primary_display, &bounds)) {
        return;
    }

    const int x = bounds.x + (bounds.w - kWindowWidth) / 2;
    const int y = bounds.y + (bounds.h - kWindowHeight) / 2;
    SDL_SetWindowPosition(window, x, y);
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

int main() {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* render_texture = nullptr;
    splonks::Graphics graphics;
    splonks::Audio audio;

    try {
        ////////////////        GRAPHICS INIT        ////////////////
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
            ThrowSdlError("SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD) failed");
        }
        if (!splonks::InitTextSubsystem()) {
            ThrowSdlError("TTF_Init failed");
        }

        SDL_SetHint(SDL_HINT_X11_WINDOW_TYPE, kX11DialogWindowType);

        const SDL_WindowFlags window_flags =
            SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_UTILITY;
        window = SDL_CreateWindow("Splonks", kWindowWidth, kWindowHeight, window_flags);
        if (window == nullptr) {
            ThrowSdlError("SDL_CreateWindow failed");
        }

        CenterWindowOnPrimaryDisplay(window);

        renderer = SDL_CreateRenderer(window, nullptr);
        if (renderer == nullptr) {
            ThrowSdlError("SDL_CreateRenderer failed");
        }

        RebaseCwdToRepoRoot();

        try {
            const std::string sprite_assets_folder = "assets/graphics/aseprite";
            graphics = splonks::Graphics::New(renderer, sprite_assets_folder);
        } catch (const std::exception&) {
            graphics = splonks::Graphics{};
        }

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
        };

        rebuild_render_texture();

        ////////////////        AUDIO INIT        ////////////////
        try {
            const auto songs = splonks::LoadSongs();
            const auto sounds = splonks::LoadSounds();
            audio = splonks::Audio::New(songs, sounds);
        } catch (const std::exception&) {
            audio = splonks::Audio{};
        }

        ////////////////        MAIN LOOP        ////////////////
        splonks::State state = splonks::State::New();
        state.running = true;

        std::uint64_t last_ticks = SDL_GetTicks();

        while (state.running) {
            SDL_Event event{};
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT) {
                    state.running = false;
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

            splonks::ProcessInput(window, renderer, state, audio, graphics, dt);
            splonks::Step(state, audio, graphics, dt);
            splonks::Render(renderer, render_texture, state, graphics);
            audio.UpdateCurrentSongStreamData();
        }

        if (render_texture != nullptr) {
            SDL_DestroyTexture(render_texture);
            render_texture = nullptr;
        }
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
        splonks::ShutdownTextSubsystem();
        SDL_Quit();
        return 0;
    } catch (const std::exception& exception) {
        graphics.ShutdownTextures();
        graphics.ShutdownText();
        if (render_texture != nullptr) {
            SDL_DestroyTexture(render_texture);
        }
        if (renderer != nullptr) {
            SDL_DestroyRenderer(renderer);
        }
        if (window != nullptr) {
            SDL_DestroyWindow(window);
        }
        audio.Shutdown();
        splonks::ShutdownTextSubsystem();
        SDL_Quit();
        std::cerr << exception.what() << '\n';
        return 1;
    }
}

#include "audio.hpp"
#include "cli.hpp"
#include "debug/control_server.hpp"
#include "debug/playback.hpp"
#include "entities/common/common.hpp"
#include "entity/archetype.hpp"
#include "graphics.hpp"
#include "imgui_layer.hpp"
#include "inputs.hpp"
#include "network/net_lobby.hpp"
#include "render/render.hpp"
#include "render/postfx.hpp"
#include "state.hpp"
#include "step.hpp"
#include "stage_acoustics.hpp"
#include "stage_lighting.hpp"
#include "tools/tool_archetype.hpp"
#include "text.hpp"

#include <SDL3/SDL.h>
#include <filesystem>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

constexpr int kWindowWidth = 1920;
constexpr int kWindowHeight = 540;
constexpr std::uint16_t kDefaultMultiplayerPort = 39000;

enum class StartupNetworkMode {
    None,
    Host,
    Join,
};

struct StartupNetworkConfig {
    StartupNetworkMode mode = StartupNetworkMode::None;
    std::string host = "127.0.0.1";
    std::uint16_t port = kDefaultMultiplayerPort;
};

std::uint16_t ParsePortArg(const char* text, std::uint16_t fallback) {
    if (text == nullptr) {
        return fallback;
    }
    try {
        const int port = std::stoi(text);
        if (port <= 0 || port > 65535) {
            return fallback;
        }
        return static_cast<std::uint16_t>(port);
    } catch (...) {
        return fallback;
    }
}

StartupNetworkConfig ParseStartupNetworkConfig(int argc, char** argv) {
    StartupNetworkConfig config;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i] != nullptr ? argv[i] : "";
        if (arg == "--multiplayer-host" || arg == "--mp-host") {
            config.mode = StartupNetworkMode::Host;
            if (i + 1 < argc) {
                config.port = ParsePortArg(argv[++i], config.port);
            }
            continue;
        }
        if (arg == "--multiplayer-join" || arg == "--mp-join") {
            config.mode = StartupNetworkMode::Join;
            if (i + 1 < argc) {
                config.host = argv[++i] != nullptr ? argv[i] : config.host;
            }
            if (i + 1 < argc) {
                config.port = ParsePortArg(argv[++i], config.port);
            }
        }
    }
    return config;
}

std::uint16_t ParseDebugControlPort(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i] != nullptr ? argv[i] : "";
        if ((arg == "--debug-control-port" || arg == "--ctl-port") && i + 1 < argc) {
            return ParsePortArg(argv[++i], 0);
        }
    }
    return 0;
}

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
    const StartupNetworkConfig startup_network = ParseStartupNetworkConfig(argc, argv);
    const std::uint16_t debug_control_port = ParseDebugControlPort(argc, argv);

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* render_texture = nullptr;
    splonks::Graphics graphics;
    splonks::RenderPostFx post_fx = splonks::RenderPostFx::New();
    splonks::Audio audio;
    splonks::DebugPlayback debug = splonks::DebugPlayback::New();
    splonks::debug::DebugControlServer debug_control_server;

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
            const auto audio_asset_db = splonks::LoadAudioAssetDb("assets/audio/annotations.yaml");
            audio = splonks::Audio::New(audio_asset_db);
            audio.music_volume = loaded_settings.audio.music_volume;
            audio.sound_effects_volume = loaded_settings.audio.sfx_volume;
            audio.SetPanHalfWidthPx(loaded_settings.audio.pan_half_width_px);
        } catch (const std::exception&) {
            audio = splonks::Audio{};
        }

        ////////////////        MAIN LOOP        ////////////////
        splonks::PopulateEntityArchetypesTable();
        splonks::SyncEntityArchetypeSizesFromFrameData(graphics);
        splonks::PopulateToolArchetypesTable();
        splonks::State state = splonks::State::New();
        state.running = true;
        debug.ui_visible = state.settings.debug_ui.menu_visible;
        debug.playback_window_visible = state.settings.debug_ui.playback_visible;
        debug.level_window_visible = state.settings.debug_ui.level_visible;
        debug.entity_inspector_visible = state.settings.debug_ui.entities_visible;
        debug.entity_annotations_visible = state.settings.debug_ui.entity_annotations_visible;
        debug.shake_brush_window_visible = state.settings.debug_ui.shake_brush_visible;
        debug.audio_brush_window_visible = state.settings.debug_ui.audio_brush_visible;
        debug.fluid_brush_window_visible = state.settings.debug_ui.fluid_brush_visible;
        debug.audio_settings_window_visible = state.settings.debug_ui.audio_settings_visible;
        debug.ui_settings_window_visible = state.settings.debug_ui.ui_settings_visible;
        debug.post_fx_settings_window_visible = state.settings.debug_ui.post_fx_settings_visible;
        debug.lighting_settings_window_visible = state.settings.debug_ui.lighting_settings_visible;
        debug.graphics_settings_window_visible = state.settings.debug_ui.graphics_settings_visible;
        debug.camera_settings_window_visible = state.settings.debug_ui.camera_settings_visible;
        debug.performance_settings_window_visible = state.settings.debug_ui.performance_settings_visible;
        debug.player_tuning_window_visible = state.settings.debug_ui.player_tuning_visible;
        if (state.settings.debug_ui.entity_swap_type > 0 &&
            state.settings.debug_ui.entity_swap_type < splonks::kEntityTypeCount) {
            debug.character_swap_entity_type =
                static_cast<splonks::EntityType>(state.settings.debug_ui.entity_swap_type);
        }
        if (state.settings.debug_ui.default_spawn_type > 0 &&
            state.settings.debug_ui.default_spawn_type < splonks::kEntityTypeCount) {
            debug.default_spawn_entity_type =
                static_cast<splonks::EntityType>(state.settings.debug_ui.default_spawn_type);
        }
        debug.default_spawn_enabled = state.settings.debug_ui.default_spawn_enabled;
        debug.character_swap_fresh = state.settings.debug_ui.entity_swap_fresh;
        debug.character_swap_keep_passives = state.settings.debug_ui.entity_swap_keep_passives;
        debug.character_swap_keep_money = state.settings.debug_ui.entity_swap_keep_money;
        debug.character_swap_keep_health = state.settings.debug_ui.entity_swap_keep_health;
        debug.character_swap_keep_tools = state.settings.debug_ui.entity_swap_keep_tools;
        splonks::RefreshRenderPostFx(post_fx, render_texture, state.settings.post_process);
        graphics.ResetTileVariations();
        splonks::InvalidateStageLighting(state);
        splonks::InvalidateStageAcoustics(state);
        if (startup_network.mode != StartupNetworkMode::None) {
            std::string network_status;
            const bool started = startup_network.mode == StartupNetworkMode::Host
                ? splonks::network::StartHostSession(state, startup_network.port, &network_status)
                : splonks::network::JoinHostSession(
                      state,
                      startup_network.host,
                      startup_network.port,
                      &network_status
                  );
            debug.network_window_visible = true;
            debug.network_status = network_status;
            std::cerr << network_status << '\n';
            if (!started) {
                std::cerr << "Startup multiplayer mode failed; continuing offline.\n";
            }
        }
        if (debug_control_port != 0) {
            std::string debug_control_error;
            if (debug_control_server.Start(debug_control_port, &debug_control_error)) {
                std::cerr << "Debug control server listening on 127.0.0.1:"
                          << debug_control_port << '\n';
            } else {
                std::cerr << "Debug control server failed: " << debug_control_error << '\n';
            }
        }

        std::uint64_t last_ticks = SDL_GetTicks();
        const double perf_frequency = static_cast<double>(SDL_GetPerformanceFrequency());
        auto counter_to_ms = [perf_frequency](std::uint64_t start, std::uint64_t end) -> double {
            if (end <= start || perf_frequency <= 0.0) {
                return 0.0;
            }
            return (static_cast<double>(end - start) * 1000.0) / perf_frequency;
        };

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

            if (debug.frame_data_auto_reload && renderer != nullptr) {
                if (graphics.ReloadFrameDataIfChanged(renderer, &debug.frame_data_reload_status)) {
                    splonks::SyncEntityArchetypeSizesFromFrameData(graphics);
                    splonks::entities::common::RefreshAllEntityFrameDataGeometry(state, graphics);
                    state.RebuildSid(graphics);
                }
            }

            const std::uint64_t frame_begin_counter = SDL_GetPerformanceCounter();
            splonks::ImGuiLayerNewFrame();
            splonks::DrawDebugPlaybackControls(debug, state, audio, graphics, window, renderer);
            audio.music_volume = state.settings.audio.music_volume;
            audio.sound_effects_volume = state.settings.audio.sfx_volume;
            audio.SetPanHalfWidthPx(state.settings.audio.pan_half_width_px);
            const std::uint64_t step_begin_counter = SDL_GetPerformanceCounter();
            splonks::RunSimulationWithDebugControls(
                window,
                renderer,
                state,
                audio,
                graphics,
                debug,
                dt
            );
            debug_control_server.Step(state);
            const std::uint64_t step_end_counter = SDL_GetPerformanceCounter();
            splonks::DrawDebugPlaybackInspector(debug, state, graphics);
            splonks::RefreshRenderPostFx(post_fx, render_texture, state.settings.post_process);
            const std::uint64_t render_begin_counter = SDL_GetPerformanceCounter();
            splonks::Render(renderer, render_texture, post_fx, state, audio, graphics);
            const std::uint64_t render_end_counter = SDL_GetPerformanceCounter();
            splonks::UpdateDebugAudioBrush(debug, state, audio, graphics);
            const std::uint64_t imgui_begin_counter = SDL_GetPerformanceCounter();
            splonks::ImGuiLayerRender();
            const std::uint64_t imgui_end_counter = SDL_GetPerformanceCounter();
            const std::uint64_t present_begin_counter = SDL_GetPerformanceCounter();
            SDL_RenderPresent(renderer);
            const std::uint64_t present_end_counter = SDL_GetPerformanceCounter();
            audio.UpdateCurrentMusicStreamData();
            const std::uint64_t frame_end_counter = SDL_GetPerformanceCounter();

            state.performance_stats.frame_budget_ms = 1000.0 / static_cast<double>(splonks::kFramesPerSecond);
            state.performance_stats.step_ms = counter_to_ms(step_begin_counter, step_end_counter);
            state.performance_stats.render_ms = counter_to_ms(render_begin_counter, render_end_counter);
            state.performance_stats.imgui_ms = counter_to_ms(imgui_begin_counter, imgui_end_counter);
            state.performance_stats.present_ms = counter_to_ms(present_begin_counter, present_end_counter);
            state.performance_stats.frame_total_ms = counter_to_ms(frame_begin_counter, frame_end_counter);
            const double smoothing_alpha = 1.0 - std::exp(-static_cast<double>(dt) * 8.0);
            if (state.performance_stats.step_smoothed_ms == 0.0) {
                state.performance_stats.step_smoothed_ms = state.performance_stats.step_ms;
                state.performance_stats.render_smoothed_ms = state.performance_stats.render_ms;
                state.performance_stats.imgui_smoothed_ms = state.performance_stats.imgui_ms;
                state.performance_stats.present_smoothed_ms = state.performance_stats.present_ms;
                state.performance_stats.frame_total_smoothed_ms = state.performance_stats.frame_total_ms;
            } else {
                state.performance_stats.step_smoothed_ms +=
                    (state.performance_stats.step_ms - state.performance_stats.step_smoothed_ms) * smoothing_alpha;
                state.performance_stats.render_smoothed_ms +=
                    (state.performance_stats.render_ms - state.performance_stats.render_smoothed_ms) * smoothing_alpha;
                state.performance_stats.imgui_smoothed_ms +=
                    (state.performance_stats.imgui_ms - state.performance_stats.imgui_smoothed_ms) * smoothing_alpha;
                state.performance_stats.present_smoothed_ms +=
                    (state.performance_stats.present_ms - state.performance_stats.present_smoothed_ms) * smoothing_alpha;
                state.performance_stats.frame_total_smoothed_ms +=
                    (state.performance_stats.frame_total_ms - state.performance_stats.frame_total_smoothed_ms) * smoothing_alpha;
            }
            state.performance_stats.step_peak_ms = std::max(state.performance_stats.step_peak_ms, state.performance_stats.step_ms);
            state.performance_stats.render_peak_ms = std::max(state.performance_stats.render_peak_ms, state.performance_stats.render_ms);
            state.performance_stats.imgui_peak_ms = std::max(state.performance_stats.imgui_peak_ms, state.performance_stats.imgui_ms);
            state.performance_stats.present_peak_ms = std::max(state.performance_stats.present_peak_ms, state.performance_stats.present_ms);
            state.performance_stats.frame_total_peak_ms = std::max(state.performance_stats.frame_total_peak_ms, state.performance_stats.frame_total_ms);
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

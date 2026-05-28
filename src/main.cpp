#include "audio.hpp"
#include "cli.hpp"
#include "debug/control_server.hpp"
#include "debug/playback.hpp"
#include "ent/spec.hpp"
#include "ents/common/common.hpp"
#include "graphics.hpp"
#include "gubsy_shell.hpp"
#include "imgui_layer.hpp"
#include "inputs.hpp"
#include "network/net_lobby.hpp"
#include "render/postfx.hpp"
#include "render/render.hpp"
#include "stage_acoustics.hpp"
#include "stage_lighting.hpp"
#include "state.hpp"
#include "step.hpp"
#include "text.hpp"
#include "tools/tool_spec.hpp"

#include <SDL3/SDL.h>
#include <cstdlib>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

enum class StartupNetworkMode {
    None,
    Host,
    Join,
};

struct StartupNetworkConfig {
    StartupNetworkMode mode = StartupNetworkMode::None;
    std::string host = "127.0.0.1";
    std::uint16_t port = splonks::network::kDefaultMultiplayerPort;
    std::vector<splonks::PlayerId> preferred_player_ids;
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

std::vector<splonks::PlayerId> ParsePreferredPlayerIds(const char* text) {
    std::vector<splonks::PlayerId> result;
    if (text == nullptr) {
        return result;
    }
    std::string value = text;
    std::size_t begin = 0;
    while (begin < value.size()) {
        const std::size_t comma = value.find(',', begin);
        const std::string token =
            value.substr(begin, comma == std::string::npos ? std::string::npos : comma - begin);
        try {
            const int player_id = std::stoi(token);
            if (player_id > 0) {
                result.push_back(static_cast<splonks::PlayerId>(player_id));
            }
        } catch (...) {
        }
        if (comma == std::string::npos) {
            break;
        }
        begin = comma + 1;
    }
    return result;
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
            continue;
        }
        if (arg == "--preferred-player-ids" && i + 1 < argc) {
            config.preferred_player_ids = ParsePreferredPlayerIds(argv[++i]);
        }
    }
    return config;
}

std::uint16_t ParseDebugControlPort(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i] != nullptr ? argv[i] : "";
        if (arg == "--no-debug-control" || arg == "--no-ctl") {
            return 0;
        }
        if ((arg == "--debug-control-port" || arg == "--ctl-port") && i + 1 < argc) {
            return ParsePortArg(argv[++i], 0);
        }
    }
#if SPLONKS_DEFAULT_DEBUG_CONTROL
    return 41000;
#else
    return 0;
#endif
}

bool HasStartupFlag(int argc, char** argv, const std::string& flag) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i] != nullptr ? argv[i] : "";
        if (arg == flag) {
            return true;
        }
    }
    return false;
}

bool RebaseCwdToExplicitProjectRoot(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i] != nullptr ? argv[i] : "";
        if ((arg == "--project-root" || arg == "--content-root") && i + 1 < argc) {
            const std::filesystem::path root = argv[++i] != nullptr ? argv[i] : "";
            if (std::filesystem::exists(root / "assets") &&
                std::filesystem::exists(root / "data")) {
                std::filesystem::current_path(root);
                return true;
            }
        }
    }
    if (const char* env_root = std::getenv("SPLONKS_PROJECT_ROOT")) {
        const std::filesystem::path root = env_root;
        if (std::filesystem::exists(root / "assets") && std::filesystem::exists(root / "data")) {
            std::filesystem::current_path(root);
            return true;
        }
    }
    return false;
}

bool InGameMenuOpenRequested() {
    return splonks::KeyPressedEdge(SDL_SCANCODE_ESCAPE) ||
           splonks::GamepadButtonPressedEdge(SDL_GAMEPAD_BUTTON_START);
}

void UpdateInGameMenuState(splonks::State& state, splonks::gubsy_shell::Shell& shell) {
    if (state.mode != splonks::Mode::Playing) {
        return;
    }

    if (!splonks::gubsy_shell::InGameMenuOpen(shell)) {
        if (InGameMenuOpenRequested()) {
            (void)splonks::gubsy_shell::OpenInGameMenu(shell);
        }
        return;
    }

    state.suppress_gameplay_input = true;
    state.pause = state.net_session.role == splonks::network::NetRole::Offline;
}

[[noreturn]] void ThrowSdlError(const char* message) {
    throw std::runtime_error(std::string(message) + ": " + SDL_GetError());
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

bool StartDebugControlServer(
    splonks::debug::DebugControlServer& server,
    std::uint16_t base_port
) {
    if (base_port == 0) {
        return false;
    }
    for (std::uint16_t port = base_port; port < base_port + 16; ++port) {
        std::string error;
        if (server.Start(port, &error)) {
            std::cerr << "Debug control server listening on 127.0.0.1:" << port << '\n';
            return true;
        }
        if (port == base_port + 15) {
            std::cerr << "Debug control server failed: " << error << '\n';
        }
    }
    return false;
}

} // namespace

int SplonksMain(int argc, char** argv) {
    if (!RebaseCwdToExplicitProjectRoot(argc, argv)) {
        RebaseCwdToRepoRoot();
    }

    if (splonks::RunCliCommand(argc, argv)) {
        return 0;
    }
    const StartupNetworkConfig startup_network = ParseStartupNetworkConfig(argc, argv);
    const std::uint16_t debug_control_port = ParseDebugControlPort(argc, argv);
    const bool debug_random_primary_input =
        HasStartupFlag(argc, argv, "--debug-random-primary-input");

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* render_texture = nullptr;
    splonks::Graphics graphics;
    splonks::RenderPostFx post_fx = splonks::RenderPostFx::New();
    splonks::Audio audio;
    splonks::DebugPlayback debug = splonks::DebugPlayback::New();
    splonks::debug::DebugControlServer debug_control_server;
    splonks::gubsy_shell::Shell gubsy_shell;

    try {
        ////////////////        SHELL INIT        ////////////////
        const splonks::Settings loaded_settings = splonks::LoadSettings();

        splonks::PopulateEntSpecsTable();
        splonks::PopulateToolSpecsTable();
        splonks::State state = splonks::State::New();
        state.running = true;
        state.gubsy_shell_ui_active = true;

        if (!splonks::gubsy_shell::InitOwned(gubsy_shell, state, loaded_settings)) {
            ThrowSdlError("gubsy_shell::InitOwned failed");
        }

        GubsyFrame gubsy_frame = splonks::gubsy_shell::GetFrame(gubsy_shell);
        window = gubsy_frame.window;
        renderer = gubsy_frame.renderer;
        render_texture = gubsy_frame.render_target;
        if (window == nullptr || renderer == nullptr || render_texture == nullptr) {
            ThrowSdlError("Gubsy frame was incomplete");
        }
        if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
            ThrowSdlError("SDL_InitSubSystem(SDL_INIT_GAMEPAD) failed");
        }
        if (!splonks::InitTextSubsystem()) {
            ThrowSdlError("TTF_Init failed");
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
        graphics.window_dims =
            splonks::UVec2::New(static_cast<unsigned int>(std::max(gubsy_frame.window_width, 1)),
                                static_cast<unsigned int>(std::max(gubsy_frame.window_height, 1)));
        graphics.dims =
            splonks::UVec2::New(static_cast<unsigned int>(std::max(gubsy_frame.render_width, 1)),
                                static_cast<unsigned int>(std::max(gubsy_frame.render_height, 1)));
        SDL_Texture* post_fx_target = nullptr;

        const auto sync_gubsy_frame = [&](const splonks::PostProcessSettings& post_settings) {
            gubsy_frame = splonks::gubsy_shell::GetFrame(gubsy_shell);
            if (gubsy_frame.window == nullptr || gubsy_frame.renderer == nullptr ||
                gubsy_frame.render_target == nullptr) {
                ThrowSdlError("Gubsy frame was incomplete");
            }
            window = gubsy_frame.window;
            renderer = gubsy_frame.renderer;
            const bool target_changed = render_texture != gubsy_frame.render_target;
            render_texture = gubsy_frame.render_target;
            graphics.window_dims = splonks::UVec2::New(
                static_cast<unsigned int>(std::max(gubsy_frame.window_width, 1)),
                static_cast<unsigned int>(std::max(gubsy_frame.window_height, 1)));
            graphics.dims = splonks::UVec2::New(
                static_cast<unsigned int>(std::max(gubsy_frame.render_width, 1)),
                static_cast<unsigned int>(std::max(gubsy_frame.render_height, 1)));
            graphics.camera.offset = splonks::ToVec2(graphics.dims / 2U);
            if (target_changed || post_fx_target != render_texture) {
                splonks::InitRenderPostFx(post_fx, renderer, render_texture, post_settings);
                post_fx_target = render_texture;
                graphics.gpu_renderer_active = post_fx.gpu_renderer_active;
            }
        };

        splonks::InitRenderPostFx(post_fx, renderer, render_texture, loaded_settings.post_process);
        post_fx_target = render_texture;
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

        splonks::SyncEntSpecSizesFromAFrame(graphics);
        ////////////////        MAIN LOOP        ////////////////
        state.debug_primary_player_bot_enabled = debug_random_primary_input;
        debug.ui_visible = state.settings.debug_ui.menu_visible;
        debug.playback_window_visible = state.settings.debug_ui.playback_visible;
        debug.level_window_visible = state.settings.debug_ui.level_visible;
        debug.ent_inspector_visible = state.settings.debug_ui.ents_visible;
        debug.ent_annotations_visible = state.settings.debug_ui.ent_annotations_visible;
        debug.shake_brush_window_visible = state.settings.debug_ui.shake_brush_visible;
        debug.audio_brush_window_visible = state.settings.debug_ui.audio_brush_visible;
        debug.fluid_brush_window_visible = state.settings.debug_ui.fluid_brush_visible;
        debug.audio_settings_window_visible = state.settings.debug_ui.audio_settings_visible;
        debug.ui_settings_window_visible = state.settings.debug_ui.ui_settings_visible;
        debug.post_fx_settings_window_visible = state.settings.debug_ui.post_fx_settings_visible;
        debug.lighting_settings_window_visible = state.settings.debug_ui.lighting_settings_visible;
        debug.graphics_settings_window_visible = state.settings.debug_ui.graphics_settings_visible;
        debug.camera_settings_window_visible = state.settings.debug_ui.camera_settings_visible;
        debug.performance_settings_window_visible =
            state.settings.debug_ui.performance_settings_visible;
        debug.player_tuning_window_visible = state.settings.debug_ui.player_tuning_visible;
        if (state.settings.debug_ui.ent_swap_type > 0 &&
            state.settings.debug_ui.ent_swap_type < splonks::kEntTypeCount) {
            debug.character_swap_ent_type =
                static_cast<splonks::EntType>(state.settings.debug_ui.ent_swap_type);
        }
        if (state.settings.debug_ui.default_spawn_type > 0 &&
            state.settings.debug_ui.default_spawn_type < splonks::kEntTypeCount) {
            debug.default_spawn_ent_type =
                static_cast<splonks::EntType>(state.settings.debug_ui.default_spawn_type);
        }
        debug.default_spawn_enabled = state.settings.debug_ui.default_spawn_enabled;
        debug.character_swap_fresh = state.settings.debug_ui.ent_swap_fresh;
        debug.character_swap_keep_passives = state.settings.debug_ui.ent_swap_keep_passives;
        debug.character_swap_keep_money = state.settings.debug_ui.ent_swap_keep_money;
        debug.character_swap_keep_health = state.settings.debug_ui.ent_swap_keep_health;
        debug.character_swap_keep_tools = state.settings.debug_ui.ent_swap_keep_tools;
        splonks::RefreshRenderPostFx(post_fx, render_texture, state.settings.post_process);
        graphics.ResetTileVariations();
        splonks::InvalidateStageLighting(state);
        splonks::InvalidateStageAcoustics(state);
        if (startup_network.mode != StartupNetworkMode::None) {
            std::string network_status;
            const bool started = startup_network.mode == StartupNetworkMode::Host
                                     ? splonks::network::StartHostSession(
                                           state, startup_network.port, &network_status)
                                     : splonks::network::JoinHostSession(
                                           state, startup_network.host, startup_network.port,
                                           startup_network.preferred_player_ids, &network_status);
            debug.network_window_visible = true;
            debug.network_status = network_status;
            std::cerr << network_status << '\n';
            if (!started) {
                std::cerr << "Startup multiplayer mode failed; continuing offline.\n";
            }
        }
        (void)StartDebugControlServer(debug_control_server, debug_control_port);

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
                splonks::gubsy_shell::ProcessEvent(gubsy_shell, event);
                if (event.type == SDL_EVENT_QUIT) {
                    state.running = false;
                } else if (event.type == SDL_EVENT_WINDOW_RESIZED ||
                           event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
                    sync_gubsy_frame(state.settings.post_process);
                }
            }

            const std::uint64_t now = SDL_GetTicks();
            const float dt = static_cast<float>(now - last_ticks) / 1000.0F;
            last_ticks = now;

            if (state.rebuild_render_texture) {
                sync_gubsy_frame(state.settings.post_process);
                state.rebuild_render_texture = false;
            }
            sync_gubsy_frame(state.settings.post_process);

            if (debug.aframe_auto_reload && renderer != nullptr) {
                if (graphics.ReloadAFrameIfChanged(renderer, &debug.aframe_reload_status)) {
                    splonks::SyncEntSpecSizesFromAFrame(graphics);
                    splonks::ents::common::RefreshAllEntAFrameGeometry(state, graphics);
                    state.RebuildSid(graphics);
                }
            }

            const std::uint64_t frame_begin_counter = SDL_GetPerformanceCounter();
            splonks::ImGuiLayerNewFrame();
            splonks::gubsy_shell::BeginDebugFrame(gubsy_shell, dt);
            splonks::gubsy_shell::UpdateDeviceState(gubsy_shell);
            splonks::gubsy_shell::ApplyLobbyGameplayInput(gubsy_shell);
            UpdateInGameMenuState(state, gubsy_shell);
            splonks::DrawDebugPlaybackControls(debug, state, audio, graphics, window, renderer);
            audio.music_volume = state.settings.audio.music_volume;
            audio.sound_effects_volume = state.settings.audio.sfx_volume;
            audio.SetPanHalfWidthPx(state.settings.audio.pan_half_width_px);
            state.performance_stats.network_pump_ms = 0.0;
            state.performance_stats.lockstep_hash_ms = 0.0;
            state.performance_stats.lockstep_hash_normal_ms = 0.0;
            state.performance_stats.lockstep_hash_rollback_ms = 0.0;
            state.performance_stats.lockstep_hash_count_this_frame = 0;
            state.performance_stats.lockstep_hash_rollback_count_this_frame = 0;
            state.performance_stats.rollback_snapshot_save_ms = 0.0;
            state.performance_stats.rollback_snapshot_restore_ms = 0.0;
            state.performance_stats.rollback_replay_ms_this_frame = 0.0;
            state.performance_stats.rollback_replay_frames_this_frame = 0;
            state.performance_stats.rollback_replay_ms_per_frame = 0.0;
            const std::uint64_t step_begin_counter = SDL_GetPerformanceCounter();
            splonks::RunSimulationWithDebugControls(window, renderer, state, audio, graphics, debug,
                                                    dt);
            if (state.mode == splonks::Mode::Title) {
                splonks::gubsy_shell::UpdateTitleMenu(gubsy_shell, state, graphics, dt,
                                                      static_cast<int>(graphics.window_dims.x),
                                                      static_cast<int>(graphics.window_dims.y));
            } else if (splonks::gubsy_shell::InGameMenuOpen(gubsy_shell)) {
                splonks::gubsy_shell::UpdateMenu(gubsy_shell, state, dt,
                                                 static_cast<int>(graphics.window_dims.x),
                                                 static_cast<int>(graphics.window_dims.y));
            }
            debug_control_server.Step(state);
            const std::uint64_t step_end_counter = SDL_GetPerformanceCounter();
            splonks::DrawDebugPlaybackInspector(debug, state, graphics);
            splonks::RefreshRenderPostFx(post_fx, render_texture, state.settings.post_process);
            const std::uint64_t render_begin_counter = SDL_GetPerformanceCounter();
            splonks::Render(renderer, render_texture, post_fx, state, audio, graphics);
            splonks::gubsy_shell::DrawFrameToWindow(gubsy_shell);
            if (state.mode == splonks::Mode::Title) {
                splonks::gubsy_shell::RenderTitleMenu(gubsy_shell, renderer,
                                                      static_cast<int>(graphics.window_dims.x),
                                                      static_cast<int>(graphics.window_dims.y));
            } else if (splonks::gubsy_shell::InGameMenuOpen(gubsy_shell)) {
                splonks::gubsy_shell::RenderMenu(gubsy_shell, renderer,
                                                 static_cast<int>(graphics.window_dims.x),
                                                 static_cast<int>(graphics.window_dims.y));
            }
            splonks::gubsy_shell::RenderAlerts(gubsy_shell, renderer,
                                               static_cast<int>(graphics.window_dims.x));
            const std::uint64_t render_end_counter = SDL_GetPerformanceCounter();
            splonks::UpdateDebugAudioBrush(debug, state, audio, graphics);
            const std::uint64_t imgui_begin_counter = SDL_GetPerformanceCounter();
            splonks::gubsy_shell::RenderDebug(gubsy_shell, renderer,
                                              static_cast<int>(graphics.window_dims.x),
                                              static_cast<int>(graphics.window_dims.y));
            splonks::ImGuiLayerRender();
            const std::uint64_t imgui_end_counter = SDL_GetPerformanceCounter();
            const std::uint64_t present_begin_counter = SDL_GetPerformanceCounter();
            splonks::gubsy_shell::PresentFrame(gubsy_shell);
            const std::uint64_t present_end_counter = SDL_GetPerformanceCounter();
            audio.UpdateCurrentMusicStreamData();
            const std::uint64_t frame_end_counter = SDL_GetPerformanceCounter();
            std::uint64_t capped_frame_end_counter = frame_end_counter;
            const int frame_cap_fps = splonks::gubsy_shell::ConfiguredFrameCapFps(gubsy_shell);
            if (frame_cap_fps > 0) {
                const double frame_seconds =
                    static_cast<double>(frame_end_counter - frame_begin_counter) / perf_frequency;
                const double target_seconds = 1.0 / static_cast<double>(frame_cap_fps);
                if (frame_seconds < target_seconds) {
                    const Uint32 delay_ms =
                        static_cast<Uint32>((target_seconds - frame_seconds) * 1000.0);
                    if (delay_ms > 0) {
                        SDL_Delay(delay_ms);
                        capped_frame_end_counter = SDL_GetPerformanceCounter();
                    }
                }
            }

            state.performance_stats.frame_budget_ms =
                1000.0 / static_cast<double>(splonks::kFramesPerSecond);
            state.performance_stats.step_ms = counter_to_ms(step_begin_counter, step_end_counter);
            state.performance_stats.multiplayer_sim_total_ms = state.performance_stats.step_ms;
            if (state.performance_stats.rollback_replay_frames_this_frame > 0) {
                state.performance_stats.rollback_replay_ms_per_frame =
                    state.performance_stats.rollback_replay_ms_this_frame /
                    static_cast<double>(state.performance_stats.rollback_replay_frames_this_frame);
            }
            state.performance_stats.render_ms =
                counter_to_ms(render_begin_counter, render_end_counter);
            state.performance_stats.imgui_ms =
                counter_to_ms(imgui_begin_counter, imgui_end_counter);
            state.performance_stats.present_ms =
                counter_to_ms(present_begin_counter, present_end_counter);
            state.performance_stats.frame_total_ms =
                counter_to_ms(frame_begin_counter, capped_frame_end_counter);
            const double smoothing_alpha = 1.0 - std::exp(-static_cast<double>(dt) * 8.0);
            if (state.performance_stats.step_smoothed_ms == 0.0) {
                state.performance_stats.step_smoothed_ms = state.performance_stats.step_ms;
                state.performance_stats.network_pump_smoothed_ms =
                    state.performance_stats.network_pump_ms;
                state.performance_stats.lockstep_hash_smoothed_ms =
                    state.performance_stats.lockstep_hash_ms;
                state.performance_stats.rollback_replay_smoothed_ms =
                    state.performance_stats.rollback_replay_ms_this_frame;
                state.performance_stats.multiplayer_sim_total_smoothed_ms =
                    state.performance_stats.multiplayer_sim_total_ms;
                state.performance_stats.render_smoothed_ms = state.performance_stats.render_ms;
                state.performance_stats.imgui_smoothed_ms = state.performance_stats.imgui_ms;
                state.performance_stats.present_smoothed_ms = state.performance_stats.present_ms;
                state.performance_stats.frame_total_smoothed_ms =
                    state.performance_stats.frame_total_ms;
            } else {
                state.performance_stats.step_smoothed_ms +=
                    (state.performance_stats.step_ms - state.performance_stats.step_smoothed_ms) *
                    smoothing_alpha;
                state.performance_stats.network_pump_smoothed_ms +=
                    (state.performance_stats.network_pump_ms -
                     state.performance_stats.network_pump_smoothed_ms) *
                    smoothing_alpha;
                state.performance_stats.lockstep_hash_smoothed_ms +=
                    (state.performance_stats.lockstep_hash_ms -
                     state.performance_stats.lockstep_hash_smoothed_ms) *
                    smoothing_alpha;
                state.performance_stats.rollback_replay_smoothed_ms +=
                    (state.performance_stats.rollback_replay_ms_this_frame -
                     state.performance_stats.rollback_replay_smoothed_ms) *
                    smoothing_alpha;
                state.performance_stats.multiplayer_sim_total_smoothed_ms +=
                    (state.performance_stats.multiplayer_sim_total_ms -
                     state.performance_stats.multiplayer_sim_total_smoothed_ms) *
                    smoothing_alpha;
                state.performance_stats.render_smoothed_ms +=
                    (state.performance_stats.render_ms -
                     state.performance_stats.render_smoothed_ms) *
                    smoothing_alpha;
                state.performance_stats.imgui_smoothed_ms +=
                    (state.performance_stats.imgui_ms - state.performance_stats.imgui_smoothed_ms) *
                    smoothing_alpha;
                state.performance_stats.present_smoothed_ms +=
                    (state.performance_stats.present_ms -
                     state.performance_stats.present_smoothed_ms) *
                    smoothing_alpha;
                state.performance_stats.frame_total_smoothed_ms +=
                    (state.performance_stats.frame_total_ms -
                     state.performance_stats.frame_total_smoothed_ms) *
                    smoothing_alpha;
            }
            state.performance_stats.step_peak_ms =
                std::max(state.performance_stats.step_peak_ms, state.performance_stats.step_ms);
            state.performance_stats.network_pump_peak_ms =
                std::max(state.performance_stats.network_pump_peak_ms,
                         state.performance_stats.network_pump_ms);
            state.performance_stats.lockstep_hash_peak_ms =
                std::max(state.performance_stats.lockstep_hash_peak_ms,
                         state.performance_stats.lockstep_hash_ms);
            state.performance_stats.rollback_replay_peak_ms =
                std::max(state.performance_stats.rollback_replay_peak_ms,
                         state.performance_stats.rollback_replay_ms_this_frame);
            state.performance_stats.multiplayer_sim_total_peak_ms =
                std::max(state.performance_stats.multiplayer_sim_total_peak_ms,
                         state.performance_stats.multiplayer_sim_total_ms);
            state.performance_stats.render_peak_ms =
                std::max(state.performance_stats.render_peak_ms, state.performance_stats.render_ms);
            state.performance_stats.imgui_peak_ms =
                std::max(state.performance_stats.imgui_peak_ms, state.performance_stats.imgui_ms);
            state.performance_stats.present_peak_ms = std::max(
                state.performance_stats.present_peak_ms, state.performance_stats.present_ms);
            state.performance_stats.frame_total_peak_ms =
                std::max(state.performance_stats.frame_total_peak_ms,
                         state.performance_stats.frame_total_ms);
        }

        post_fx.Shutdown();
        graphics.ShutdownTextures();
        graphics.ShutdownText();
        audio.Shutdown();
        splonks::ShutdownImGuiLayer();
        splonks::gubsy_shell::Shutdown(gubsy_shell);
        render_texture = nullptr;
        renderer = nullptr;
        window = nullptr;
        splonks::ShutdownTextSubsystem();
        SDL_Quit();
        return 0;
    } catch (const std::exception& exception) {
        const std::string error_message = exception.what();
        post_fx.Shutdown();
        graphics.ShutdownTextures();
        graphics.ShutdownText();
        splonks::ShutdownImGuiLayer();
        splonks::gubsy_shell::Shutdown(gubsy_shell);
        render_texture = nullptr;
        renderer = nullptr;
        window = nullptr;
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

#ifdef __ANDROID__
extern "C" int SDL_main(int argc, char** argv) {
    return SplonksMain(argc, argv);
}
#endif

int main(int argc, char** argv) {
    return SplonksMain(argc, argv);
}

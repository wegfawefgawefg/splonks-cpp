#include "debug_playback.hpp"

#include "entity.hpp"
#include "frame_data.hpp"
#include "imgui_layer.hpp"
#include "inputs.hpp"
#include "stage_init.hpp"
#include "step.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <type_traits>

namespace {

using namespace splonks;

constexpr float kMinTimeScale = 0.01F;
constexpr float kMaxTimeScale = 2.0F;
constexpr int kMinSnapshots = 1;
constexpr int kMaxSnapshots = 20000;
constexpr std::uint32_t kRecordingMagic = 0x53504C52U;
constexpr std::uint32_t kRecordingVersion = 6;

template <typename T>
void WritePod(std::ostream& out, const T& value) {
    static_assert(std::is_trivially_copyable_v<T>);
    out.write(reinterpret_cast<const char*>(&value), static_cast<std::streamsize>(sizeof(T)));
}

template <typename T>
bool ReadPod(std::istream& in, T& value) {
    static_assert(std::is_trivially_copyable_v<T>);
    in.read(reinterpret_cast<char*>(&value), static_cast<std::streamsize>(sizeof(T)));
    return in.good();
}

template <typename T>
void WriteVectorPod(std::ostream& out, const std::vector<T>& values) {
    static_assert(std::is_trivially_copyable_v<T>);
    const std::uint32_t count = static_cast<std::uint32_t>(values.size());
    WritePod(out, count);
    if (!values.empty()) {
        out.write(reinterpret_cast<const char*>(values.data()),
                  static_cast<std::streamsize>(sizeof(T) * values.size()));
    }
}

template <typename T>
bool ReadVectorPod(std::istream& in, std::vector<T>& values) {
    static_assert(std::is_trivially_copyable_v<T>);
    std::uint32_t count = 0;
    if (!ReadPod(in, count)) {
        return false;
    }
    values.resize(count);
    if (count > 0) {
        in.read(reinterpret_cast<char*>(values.data()),
                static_cast<std::streamsize>(sizeof(T) * values.size()));
    }
    return in.good();
}

template <typename T>
void WriteOptionalPod(std::ostream& out, const std::optional<T>& value) {
    static_assert(std::is_trivially_copyable_v<T>);
    const bool has_value = value.has_value();
    WritePod(out, has_value);
    if (has_value) {
        WritePod(out, *value);
    }
}

template <typename T>
bool ReadOptionalPod(std::istream& in, std::optional<T>& value) {
    static_assert(std::is_trivially_copyable_v<T>);
    bool has_value = false;
    if (!ReadPod(in, has_value)) {
        return false;
    }
    if (!has_value) {
        value.reset();
        return true;
    }
    T loaded{};
    if (!ReadPod(in, loaded)) {
        return false;
    }
    value = loaded;
    return true;
}

const char* ModeToString(Mode mode) {
    switch (mode) {
    case Mode::Title:
        return "Title";
    case Mode::Settings:
        return "Settings";
    case Mode::VideoSettings:
        return "VideoSettings";
    case Mode::Playing:
        return "Playing";
    case Mode::StageTransition:
        return "StageTransition";
    case Mode::GameOver:
        return "GameOver";
    case Mode::Win:
        return "Win";
    }
    return "Unknown";
}

const char* DebugLevelKindToString(DebugLevelKind kind) {
    switch (kind) {
    case DebugLevelKind::Test1:
        return "Test1";
    case DebugLevelKind::HangTest:
        return "HangTest";
    }
    return "Unknown";
}

const char* EntityTypeToString(EntityType type) {
    switch (type) {
    case EntityType::None:
        return "None";
    case EntityType::Player:
        return "Player";
    case EntityType::Bat:
        return "Bat";
    case EntityType::Rock:
        return "Rock";
    case EntityType::Pot:
        return "Pot";
    case EntityType::Box:
        return "Box";
    case EntityType::Block:
        return "Block";
    case EntityType::Bomb:
        return "Bomb";
    case EntityType::JetPack:
        return "JetPack";
    case EntityType::Rope:
        return "Rope";
    case EntityType::BaseballBat:
        return "BaseballBat";
    case EntityType::GhostBall:
        return "GhostBall";
    case EntityType::MouseTrailer:
        return "MouseTrailer";
    case EntityType::Gold:
        return "Gold";
    case EntityType::GoldStack:
        return "GoldStack";
    }
    return "Unknown";
}

const char* DisplayStateToString(EntityDisplayState state) {
    switch (state) {
    case EntityDisplayState::Neutral:
        return "Neutral";
    case EntityDisplayState::NeutralHolding:
        return "NeutralHolding";
    case EntityDisplayState::Walk:
        return "Walk";
    case EntityDisplayState::WalkHolding:
        return "WalkHolding";
    case EntityDisplayState::Fly:
        return "Fly";
    case EntityDisplayState::Dead:
        return "Dead";
    case EntityDisplayState::Stunned:
        return "Stunned";
    case EntityDisplayState::Climbing:
        return "Climbing";
    case EntityDisplayState::Hanging:
        return "Hanging";
    case EntityDisplayState::Falling:
        return "Falling";
    }
    return "Unknown";
}

const char* SuperStateToString(EntitySuperState state) {
    switch (state) {
    case EntitySuperState::Idle:
        return "Idle";
    case EntitySuperState::Dead:
        return "Dead";
    case EntitySuperState::Crushed:
        return "Crushed";
    case EntitySuperState::Stunned:
        return "Stunned";
    case EntitySuperState::Pursuing:
        return "Pursuing";
    case EntitySuperState::Attacking:
        return "Attacking";
    case EntitySuperState::Defending:
        return "Defending";
    case EntitySuperState::Fleeing:
        return "Fleeing";
    case EntitySuperState::Searching:
        return "Searching";
    case EntitySuperState::Patrolling:
        return "Patrolling";
    case EntitySuperState::Roaming:
        return "Roaming";
    case EntitySuperState::Returning:
        return "Returning";
    case EntitySuperState::Projectile:
        return "Projectile";
    case EntitySuperState::EquippedToBack:
        return "EquippedToBack";
    }
    return "Unknown";
}

const char* LeftOrRightToString(LeftOrRight facing) {
    switch (facing) {
    case LeftOrRight::Left:
        return "Left";
    case LeftOrRight::Right:
        return "Right";
    }
    return "Unknown";
}

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
}

void WriteSettings(std::ostream& out, const Settings& settings) {
    WritePod(out, settings.mode);
    WritePod(out, settings.video.resolution);
    WritePod(out, settings.video.fullscreen);
    WritePod(out, settings.video.vsync);
    WriteVectorPod(out, settings.video.resolution_options);
    WritePod(out, settings.audio.music_volume);
    WritePod(out, settings.audio.sfx_volume);
    WritePod(out, settings.controls.jump);
    WritePod(out, settings.controls.shoot);
}

bool ReadSettings(std::istream& in, Settings& settings) {
    if (!ReadPod(in, settings.mode) ||
        !ReadPod(in, settings.video.resolution) ||
        !ReadPod(in, settings.video.fullscreen) ||
        !ReadPod(in, settings.video.vsync) ||
        !ReadVectorPod(in, settings.video.resolution_options) ||
        !ReadPod(in, settings.audio.music_volume) ||
        !ReadPod(in, settings.audio.sfx_volume) ||
        !ReadPod(in, settings.controls.jump) ||
        !ReadPod(in, settings.controls.shoot)) {
        return false;
    }
    return true;
}

void WriteStage(std::ostream& out, const Stage& stage) {
    WritePod(out, stage.stage_type);
    WritePod(out, stage.gravity);
    WritePod(out, stage.camera_clamp_margin);
    const std::uint32_t tile_rows = static_cast<std::uint32_t>(stage.tiles.size());
    WritePod(out, tile_rows);
    for (const std::vector<Tile>& row : stage.tiles) {
        WriteVectorPod(out, row);
    }

    const std::uint32_t room_rows = static_cast<std::uint32_t>(stage.rooms.size());
    WritePod(out, room_rows);
    for (const std::vector<int>& row : stage.rooms) {
        WriteVectorPod(out, row);
    }

    WriteVectorPod(out, stage.path);
}

bool ReadStage(std::istream& in, Stage& stage) {
    if (!ReadPod(in, stage.stage_type) ||
        !ReadPod(in, stage.gravity) ||
        !ReadPod(in, stage.camera_clamp_margin)) {
        return false;
    }

    std::uint32_t tile_rows = 0;
    if (!ReadPod(in, tile_rows)) {
        return false;
    }
    stage.tiles.resize(tile_rows);
    for (std::uint32_t i = 0; i < tile_rows; ++i) {
        if (!ReadVectorPod(in, stage.tiles[i])) {
            return false;
        }
    }

    std::uint32_t room_rows = 0;
    if (!ReadPod(in, room_rows)) {
        return false;
    }
    stage.rooms.resize(room_rows);
    for (std::uint32_t i = 0; i < room_rows; ++i) {
        if (!ReadVectorPod(in, stage.rooms[i])) {
            return false;
        }
    }

    return ReadVectorPod(in, stage.path);
}

void WriteEntityManager(std::ostream& out, const EntityManager& entity_manager) {
    WriteVectorPod(out, entity_manager.entities);
    WriteVectorPod(out, entity_manager.available_ids);
}

bool ReadEntityManager(std::istream& in, EntityManager& entity_manager) {
    return ReadVectorPod(in, entity_manager.entities) &&
           ReadVectorPod(in, entity_manager.available_ids);
}

void WriteSnapshot(std::ostream& out, const GameplaySnapshot& snapshot) {
    WritePod(out, snapshot.mode);
    WriteSettings(out, snapshot.settings);
    WritePod(out, snapshot.menu_inputs);
    WritePod(out, snapshot.menu_input_debounce_timers);
    WritePod(out, snapshot.playing_inputs);
    WritePod(out, snapshot.title_menu_selection);
    WritePod(out, snapshot.settings_menu_selection);
    WritePod(out, snapshot.video_settings_menu_selection);
    WriteOptionalPod(out, snapshot.video_settings_target_window_size_index);
    WriteOptionalPod(out, snapshot.video_settings_target_resolution_index);
    WriteOptionalPod(out, snapshot.video_settings_target_fullscreen);
    WritePod(out, snapshot.rebuild_render_texture);
    WritePod(out, snapshot.choosing_control_binding);
    WritePod(out, snapshot.show_entity_collision_boxes);
    WritePod(out, snapshot.now);
    WritePod(out, snapshot.time_since_last_update);
    WritePod(out, snapshot.scene_frame);
    WritePod(out, snapshot.frame);
    WritePod(out, snapshot.stage_frame);
    WritePod(out, snapshot.menu_return_to);
    WritePod(out, snapshot.game_over);
    WritePod(out, snapshot.pause);
    WritePod(out, snapshot.win);
    WriteOptionalPod(out, snapshot.next_stage);
    WritePod(out, snapshot.points);
    WritePod(out, snapshot.deaths);
    WritePod(out, snapshot.frame_pause);
    WritePod(out, snapshot.debug_level);
    WriteEntityManager(out, snapshot.entity_manager);
    WriteStage(out, snapshot.stage);
    WriteOptionalPod(out, snapshot.player_vid);
    WriteOptionalPod(out, snapshot.mouse_trailer_vid);
    WritePod(out, snapshot.play_cam_pos);
}

bool ReadSnapshot(std::istream& in, GameplaySnapshot& snapshot) {
    return ReadPod(in, snapshot.mode) &&
           ReadSettings(in, snapshot.settings) &&
           ReadPod(in, snapshot.menu_inputs) &&
           ReadPod(in, snapshot.menu_input_debounce_timers) &&
           ReadPod(in, snapshot.playing_inputs) &&
           ReadPod(in, snapshot.title_menu_selection) &&
           ReadPod(in, snapshot.settings_menu_selection) &&
           ReadPod(in, snapshot.video_settings_menu_selection) &&
           ReadOptionalPod(in, snapshot.video_settings_target_window_size_index) &&
           ReadOptionalPod(in, snapshot.video_settings_target_resolution_index) &&
           ReadOptionalPod(in, snapshot.video_settings_target_fullscreen) &&
           ReadPod(in, snapshot.rebuild_render_texture) &&
           ReadPod(in, snapshot.choosing_control_binding) &&
           ReadPod(in, snapshot.show_entity_collision_boxes) &&
           ReadPod(in, snapshot.now) &&
           ReadPod(in, snapshot.time_since_last_update) &&
           ReadPod(in, snapshot.scene_frame) &&
           ReadPod(in, snapshot.frame) &&
           ReadPod(in, snapshot.stage_frame) &&
           ReadPod(in, snapshot.menu_return_to) &&
           ReadPod(in, snapshot.game_over) &&
           ReadPod(in, snapshot.pause) &&
           ReadPod(in, snapshot.win) &&
           ReadOptionalPod(in, snapshot.next_stage) &&
           ReadPod(in, snapshot.points) &&
           ReadPod(in, snapshot.deaths) &&
           ReadPod(in, snapshot.frame_pause) &&
           ReadPod(in, snapshot.debug_level) &&
           ReadEntityManager(in, snapshot.entity_manager) &&
           ReadStage(in, snapshot.stage) &&
           ReadOptionalPod(in, snapshot.player_vid) &&
           ReadOptionalPod(in, snapshot.mouse_trailer_vid) &&
           ReadPod(in, snapshot.play_cam_pos);
}

bool SaveRecordingToFile(const DebugPlayback& debug, std::string* status_out) {
    if (debug.file_path[0] == '\0') {
        if (status_out != nullptr) {
            *status_out = "No file path set.";
        }
        return false;
    }

    std::ofstream out(debug.file_path.data(), std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        if (status_out != nullptr) {
            *status_out = "Failed to open file for writing.";
        }
        return false;
    }

    WritePod(out, kRecordingMagic);
    WritePod(out, kRecordingVersion);
    const std::uint32_t count = static_cast<std::uint32_t>(debug.recorded_snapshots.size());
    WritePod(out, count);
    for (const GameplaySnapshot& snapshot : debug.recorded_snapshots) {
        WriteSnapshot(out, snapshot);
    }

    if (!out.good()) {
        if (status_out != nullptr) {
            *status_out = "Write failed.";
        }
        return false;
    }

    if (status_out != nullptr) {
        char buffer[128];
        std::snprintf(buffer, sizeof(buffer), "Saved %u snapshots.", count);
        *status_out = buffer;
    }
    return true;
}

bool LoadRecordingFromFile(DebugPlayback& debug, std::string* status_out) {
    if (debug.file_path[0] == '\0') {
        if (status_out != nullptr) {
            *status_out = "No file path set.";
        }
        return false;
    }

    std::ifstream in(debug.file_path.data(), std::ios::binary);
    if (!in.is_open()) {
        if (status_out != nullptr) {
            *status_out = "Failed to open file for reading.";
        }
        return false;
    }

    std::uint32_t magic = 0;
    std::uint32_t version = 0;
    std::uint32_t count = 0;
    if (!ReadPod(in, magic) || !ReadPod(in, version) || !ReadPod(in, count)) {
        if (status_out != nullptr) {
            *status_out = "Failed to read recording header.";
        }
        return false;
    }
    if (magic != kRecordingMagic) {
        if (status_out != nullptr) {
            *status_out = "Recording file magic mismatch.";
        }
        return false;
    }
    if (version != kRecordingVersion) {
        if (status_out != nullptr) {
            *status_out = "Recording file version mismatch.";
        }
        return false;
    }

    std::deque<GameplaySnapshot> loaded_snapshots;
    for (std::uint32_t i = 0; i < count; ++i) {
        GameplaySnapshot snapshot;
        if (!ReadSnapshot(in, snapshot)) {
            if (status_out != nullptr) {
                *status_out = "Failed while reading snapshot data.";
            }
            return false;
        }
        loaded_snapshots.push_back(std::move(snapshot));
    }

    debug.recorded_snapshots = std::move(loaded_snapshots);
    debug.playback_index = debug.recorded_snapshots.empty() ? 0 : debug.recorded_snapshots.size() - 1;

    if (status_out != nullptr) {
        char buffer[128];
        std::snprintf(buffer, sizeof(buffer), "Loaded %u snapshots.", count);
        *status_out = buffer;
    }
    return true;
}

bool ExportRecordingToTextFile(const DebugPlayback& debug, const Graphics& graphics, std::string* status_out) {
    if (debug.file_path[0] == '\0') {
        if (status_out != nullptr) {
            *status_out = "No file path set.";
        }
        return false;
    }

    std::ofstream out(debug.file_path.data(), std::ios::trunc);
    if (!out.is_open()) {
        if (status_out != nullptr) {
            *status_out = "Failed to open file for text export.";
        }
        return false;
    }

    out << "splonks recording text export\n";
    out << "snapshots: " << debug.recorded_snapshots.size() << "\n\n";

    for (std::size_t snapshot_index = 0; snapshot_index < debug.recorded_snapshots.size(); ++snapshot_index) {
        const GameplaySnapshot& snapshot = debug.recorded_snapshots[snapshot_index];
        out << "snapshot " << snapshot_index << "\n";
        out << "  mode: " << ModeToString(snapshot.mode) << "\n";
        out << "  scene_frame: " << snapshot.scene_frame << "\n";
        out << "  frame: " << snapshot.frame << "\n";
        out << "  stage_frame: " << snapshot.stage_frame << "\n";
        out << "  stage_type: " << static_cast<int>(snapshot.stage.stage_type) << "\n";
        out << "  points: " << snapshot.points << "\n";
        out << "  deaths: " << snapshot.deaths << "\n";
        out << "  play_cam_pos: (" << snapshot.play_cam_pos.x << ", " << snapshot.play_cam_pos.y << ")\n";

        std::size_t active_count = 0;
        for (const Entity& entity : snapshot.entity_manager.entities) {
            if (entity.active) {
                active_count += 1;
            }
        }
        out << "  active_entities: " << active_count << "\n";

        for (std::size_t entity_id = 0; entity_id < snapshot.entity_manager.entities.size(); ++entity_id) {
            const Entity& entity = snapshot.entity_manager.entities[entity_id];
            if (!entity.active) {
                continue;
            }

            out << "  entity " << entity_id << "\n";
            out << "    type: " << EntityTypeToString(entity.type_) << "\n";
            out << "    display_state: " << DisplayStateToString(entity.display_state) << "\n";
            out << "    super_state: " << SuperStateToString(entity.super_state) << "\n";
            out << "    facing: " << LeftOrRightToString(entity.facing) << "\n";
            out << "    grounded: " << (entity.grounded ? "true" : "false") << "\n";
            out << "    climbing: " << (entity.climbing ? "true" : "false") << "\n";
            out << "    holding: " << (entity.holding ? "true" : "false") << "\n";
            out << "    running: " << (entity.running ? "true" : "false") << "\n";
            out << "    coyote_time: " << entity.coyote_time << "\n";
            out << "    pos: (" << entity.pos.x << ", " << entity.pos.y << ")\n";
            out << "    vel: (" << entity.vel.x << ", " << entity.vel.y << ")\n";
            out << "    acc: (" << entity.acc.x << ", " << entity.acc.y << ")\n";
            out << "    size: (" << entity.size.x << ", " << entity.size.y << ")\n";
            out << "    health: " << entity.health << "\n";
            out << "    money: " << entity.money << "\n";
            out << "    bombs: " << entity.bombs << "\n";
            out << "    ropes: " << entity.ropes << "\n";

            if (entity.frame_data_animator.HasAnimation()) {
                const FrameDataAnimation* animation =
                    graphics.frame_data_db.FindAnimation(entity.frame_data_animator.animation_id);
                if (animation != nullptr) {
                    out << "    animation: " << animation->name << "\n";
                    out << "    animation_frame: " << entity.frame_data_animator.current_frame << "\n";
                    out << "    animation_time: " << entity.frame_data_animator.current_time << "\n";
                    const FrameData* frame_data = graphics.frame_data_db.FindFrame(
                        entity.frame_data_animator.animation_id,
                        entity.frame_data_animator.current_frame
                    );
                    if (frame_data != nullptr) {
                        out << "    frame_duration: " << frame_data->duration << "\n";
                        out << "    sample_rect: (" << frame_data->sample_rect.x << ", "
                            << frame_data->sample_rect.y << ", " << frame_data->sample_rect.w
                            << ", " << frame_data->sample_rect.h << ")\n";
                        out << "    draw_offset: (" << frame_data->draw_offset.x << ", "
                            << frame_data->draw_offset.y << ")\n";
                        out << "    pbox: (" << frame_data->pbox.x << ", " << frame_data->pbox.y
                            << ", " << frame_data->pbox.w << ", " << frame_data->pbox.h << ")\n";
                        out << "    cbox: (" << frame_data->cbox.x << ", " << frame_data->cbox.y
                            << ", " << frame_data->cbox.w << ", " << frame_data->cbox.h << ")\n";
                    }
                }
            }
        }

        out << "\n";
    }

    if (!out.good()) {
        if (status_out != nullptr) {
            *status_out = "Text export failed.";
        }
        return false;
    }

    if (status_out != nullptr) {
        char buffer[128];
        std::snprintf(buffer, sizeof(buffer), "Exported %zu snapshots as text.", debug.recorded_snapshots.size());
        *status_out = buffer;
    }
    return true;
}

void DrawSimulationControls(DebugPlayback& debug, State& state, Graphics& graphics) {
    if (!debug.ui_visible) {
        if (ImGui::IsKeyPressed(ImGuiKey_F2)) {
            debug.ui_visible = true;
        }
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_F2)) {
        debug.ui_visible = false;
        return;
    }

    ImGui::SetNextWindowBgAlpha(0.9F);
    ImGui::SetNextWindowPos(ImVec2(12.0F, 12.0F), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Debug: Playback")) {
        ImGui::End();
        return;
    }

    ImGui::Text("Mode: %s", ModeToString(state.mode));
    ImGui::Text("Scene Frame: %u", state.scene_frame);
    ImGui::Text("Game Frame: %u", state.frame);
    ImGui::Text("Stage Frame: %u", state.stage_frame);
    ImGui::Text("Snapshots: %zu", debug.recorded_snapshots.size());
    ImGui::Text("Playback: %s", debug.playback_active ? "On" : "Off");
    ImGui::Separator();

    ImGui::SliderFloat("Time Scale", &debug.time_scale, kMinTimeScale, kMaxTimeScale, "%.2fx");
    debug.time_scale = std::clamp(debug.time_scale, kMinTimeScale, kMaxTimeScale);
    ImGui::Checkbox("Pause Live Simulation", &debug.pause_live_simulation);
    if (ImGui::Button("Step One Tick")) {
        debug.step_live_simulation_once = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("1x")) {
        debug.time_scale = 1.0F;
    }
    ImGui::SameLine();
    if (ImGui::Button("0.25x")) {
        debug.time_scale = 0.25F;
    }
    ImGui::SameLine();
    if (ImGui::Button("0.10x")) {
        debug.time_scale = 0.10F;
    }

    ImGui::Separator();
    ImGui::InputInt("Max Snapshots", &debug.max_snapshots);
    debug.max_snapshots = std::clamp(debug.max_snapshots, kMinSnapshots, kMaxSnapshots);

    if (!debug.playback_active) {
        if (!debug.recording) {
            if (ImGui::Button("Start Recording")) {
                StartRecording(debug, state, graphics);
            }
        } else {
            if (ImGui::Button("Stop Recording")) {
                StopRecording(debug);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear Recording")) {
            debug.recorded_snapshots.clear();
            debug.playback_index = 0;
            debug.recording = false;
        }
        if (!debug.recorded_snapshots.empty()) {
            if (ImGui::Button("Enter Playback")) {
                EnterPlayback(debug, state, graphics);
            }
        }
    } else {
        if (ImGui::Button("Exit Playback")) {
            ExitPlayback(debug, state, graphics);
        }
    }

    ImGui::Separator();
    ImGui::InputText("Recording File", debug.file_path.data(), debug.file_path.size());
    if (ImGui::Button("Save Recording")) {
        SaveRecordingToFile(debug, &debug.io_status);
    }
    ImGui::SameLine();
    if (ImGui::Button("Load Recording")) {
        LoadRecordingFromFile(debug, &debug.io_status);
    }
    ImGui::SameLine();
    if (ImGui::Button("Export Text")) {
        ExportRecordingToTextFile(debug, graphics, &debug.io_status);
    }
    if (!debug.io_status.empty()) {
        ImGui::TextWrapped("%s", debug.io_status.c_str());
    }

    if (debug.playback_active && !debug.recorded_snapshots.empty()) {
        ClampPlaybackIndex(debug);
        int playback_index = static_cast<int>(debug.playback_index);
        const int max_index = static_cast<int>(debug.recorded_snapshots.size()) - 1;
        ImGui::Separator();
        if (ImGui::Button("|<")) {
            debug.playback_index = 0;
        }
        ImGui::SameLine();
        if (ImGui::Button("<")) {
            if (debug.playback_index > 0) {
                debug.playback_index -= 1;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(">")) {
            if (debug.playback_index + 1 < debug.recorded_snapshots.size()) {
                debug.playback_index += 1;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(">|")) {
            debug.playback_index = debug.recorded_snapshots.size() - 1;
        }
        if (ImGui::SliderInt("Playback Frame", &playback_index, 0, max_index)) {
            debug.playback_index = static_cast<std::size_t>(playback_index);
        }
        ImGui::Text("Frame %zu / %zu", debug.playback_index, debug.recorded_snapshots.size() - 1);
    }

    ImGui::End();
}

void DrawLevelControls(DebugPlayback& debug, State& state, Graphics& graphics) {
    if (!debug.ui_visible) {
        return;
    }

    ImGui::SetNextWindowBgAlpha(0.9F);
    ImGui::SetNextWindowPos(ImVec2(360.0F, 12.0F), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Debug: Level")) {
        ImGui::End();
        return;
    }

    if (debug.playback_active) {
        ImGui::BeginDisabled();
    }

    int level_kind = static_cast<int>(state.debug_level.kind);
    const char* level_names[] = {"Test1", "HangTest"};
    ImGui::Combo("Preset", &level_kind, level_names, IM_ARRAYSIZE(level_names));
    state.debug_level.kind = static_cast<DebugLevelKind>(level_kind);
    ImGui::Text("Active: %s", DebugLevelKindToString(state.debug_level.kind));
    ImGui::Checkbox("Players Have Gloves", &state.debug_level.player_has_gloves);

    if (state.debug_level.kind == DebugLevelKind::HangTest) {
        HangTestLevelConfig& hang_test = state.debug_level.hang_test;
        ImGui::SliderInt("Stage Height", &hang_test.stage_height_tiles, 16, 512);
        const int cutout_drop_max =
            std::max(2, hang_test.stage_height_tiles - 8);
        const int cutout_height_max =
            std::max(1, hang_test.stage_height_tiles - 7 - hang_test.cutout_drop_tiles);

        ImGui::SliderInt("Cutout Drop", &hang_test.cutout_drop_tiles, 2, cutout_drop_max);
        ImGui::SliderInt("Cutout Height", &hang_test.cutout_height_tiles, 1, std::min(8, cutout_height_max));
    }

    if (ImGui::Button("Regenerate")) {
        InitDebugLevel(state);
        graphics.ResetTileVariations();
    }

    if (debug.playback_active) {
        ImGui::EndDisabled();
    }

    ImGui::End();
}

void DrawEntityInspector(DebugPlayback& debug, const State& state, const Graphics& graphics) {
    if (!debug.ui_visible) {
        return;
    }

    ImGui::SetNextWindowBgAlpha(0.9F);
    ImGui::SetNextWindowPos(ImVec2(12.0F, 300.0F), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Debug: Entities")) {
        ImGui::End();
        return;
    }

    if (ImGui::BeginListBox("Entities", ImVec2(260.0F, 220.0F))) {
        for (std::size_t i = 0; i < state.entity_manager.entities.size(); ++i) {
            const Entity& entity = state.entity_manager.entities[i];
            if (!entity.active) {
                continue;
            }

            char label[128];
            std::snprintf(
                label,
                sizeof(label),
                "%zu: %s##entity_%zu",
                i,
                EntityTypeToString(entity.type_),
                i
            );
            const bool selected = debug.selected_entity_id == i;
            if (ImGui::Selectable(label, selected)) {
                debug.selected_entity_id = i;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndListBox();
    }

    if (debug.selected_entity_id >= state.entity_manager.entities.size()) {
        debug.selected_entity_id = 0;
    }

    const Entity* selected_entity = nullptr;
    if (!state.entity_manager.entities.empty()) {
        const Entity& entity = state.entity_manager.entities[debug.selected_entity_id];
        if (entity.active) {
            selected_entity = &entity;
        }
    }

    if (selected_entity == nullptr) {
        ImGui::TextUnformatted("No active entity selected.");
        ImGui::End();
        return;
    }

    const Entity& entity = *selected_entity;
    const AABB aabb = entity.GetAABB();
    ImGui::Separator();
    ImGui::Text("Type: %s", EntityTypeToString(entity.type_));
    ImGui::Text("Display: %s", DisplayStateToString(entity.display_state));
    ImGui::Text("Super: %s", SuperStateToString(entity.super_state));
    ImGui::Text("Facing: %s", LeftOrRightToString(entity.facing));
    ImGui::Text("Grounded: %s", entity.grounded ? "true" : "false");
    ImGui::Text("Pos: (%.2f, %.2f)", entity.pos.x, entity.pos.y);
    ImGui::Text("Vel: (%.2f, %.2f)", entity.vel.x, entity.vel.y);
    ImGui::Text("Acc: (%.2f, %.2f)", entity.acc.x, entity.acc.y);
    ImGui::Text("Size: (%.2f, %.2f)", entity.size.x, entity.size.y);
    ImGui::Text("AABB TL: (%.2f, %.2f)", aabb.tl.x, aabb.tl.y);
    ImGui::Text("AABB BR: (%.2f, %.2f)", aabb.br.x, aabb.br.y);
    ImGui::Text("Coyote: %u", entity.coyote_time);
    ImGui::Text("Health: %u", entity.health);
    ImGui::Text("Money: %u", entity.money);
    ImGui::Text("Bombs: %u", entity.bombs);
    ImGui::Text("Ropes: %u", entity.ropes);
    ImGui::Text("Climbing: %s", entity.climbing ? "true" : "false");
    ImGui::Text("Holding: %s", entity.holding ? "true" : "false");
    ImGui::Text("Running: %s", entity.running ? "true" : "false");
    ImGui::Text("Horiz Controlled: %s", entity.IsHorizontallyControlled() ? "true" : "false");

    if (entity.frame_data_animator.HasAnimation()) {
        const FrameDataAnimation* animation =
            graphics.frame_data_db.FindAnimation(entity.frame_data_animator.animation_id);
        if (animation != nullptr) {
            ImGui::Text("Anim: %s", animation->name.c_str());
            ImGui::Text("Anim Frame: %zu / %zu",
                        entity.frame_data_animator.current_frame,
                        animation->frame_indices.empty() ? 0 : animation->frame_indices.size() - 1);
            const FrameData* frame_data = graphics.frame_data_db.FindFrame(
                entity.frame_data_animator.animation_id,
                entity.frame_data_animator.current_frame
            );
            if (frame_data != nullptr) {
                ImGui::Text("Frame Duration: %d", frame_data->duration);
                ImGui::Text("Sample: (%d, %d, %d, %d)",
                            frame_data->sample_rect.x,
                            frame_data->sample_rect.y,
                            frame_data->sample_rect.w,
                            frame_data->sample_rect.h);
                ImGui::Text("Draw Offset: (%d, %d)",
                            frame_data->draw_offset.x,
                            frame_data->draw_offset.y);
                ImGui::Text("PBox: (%d, %d, %d, %d)",
                            frame_data->pbox.x,
                            frame_data->pbox.y,
                            frame_data->pbox.w,
                            frame_data->pbox.h);
                ImGui::Text("CBox: (%d, %d, %d, %d)",
                            frame_data->cbox.x,
                            frame_data->cbox.y,
                            frame_data->cbox.w,
                            frame_data->cbox.h);
            }
        }
    }

    ImGui::End();
}

bool ShouldProcessGameplayInput(const DebugPlayback& debug) {
    if (debug.playback_active) {
        return false;
    }

    if (ImGuiWantsMouse()) {
        return false;
    }

    return true;
}

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

} // namespace

namespace splonks {

DebugPlayback DebugPlayback::New() {
    DebugPlayback result;
    const char* default_path = "debug_recording.splrec";
    std::strncpy(result.file_path.data(), default_path, result.file_path.size() - 1);
    result.file_path[result.file_path.size() - 1] = '\0';
    return result;
}

bool ConvertRecordingFileToText(
    const std::string& input_path,
    const std::string& output_path,
    const FrameDataDb& frame_data_db,
    std::string* status_out
) {
    DebugPlayback debug = DebugPlayback::New();
    std::strncpy(debug.file_path.data(), input_path.c_str(), debug.file_path.size() - 1);
    debug.file_path[debug.file_path.size() - 1] = '\0';
    if (!LoadRecordingFromFile(debug, status_out)) {
        return false;
    }

    Graphics graphics{};
    graphics.frame_data_db = frame_data_db;
    std::strncpy(debug.file_path.data(), output_path.c_str(), debug.file_path.size() - 1);
    debug.file_path[debug.file_path.size() - 1] = '\0';
    return ExportRecordingToTextFile(debug, graphics, status_out);
}

GameplaySnapshot MakeGameplaySnapshot(const State& state, const Graphics& graphics) {
    GameplaySnapshot snapshot;
    snapshot.mode = state.mode;
    snapshot.settings = state.settings;
    snapshot.menu_inputs = state.menu_inputs;
    snapshot.menu_input_debounce_timers = state.menu_input_debounce_timers;
    snapshot.playing_inputs = state.playing_inputs;
    snapshot.title_menu_selection = state.title_menu_selection;
    snapshot.settings_menu_selection = state.settings_menu_selection;
    snapshot.video_settings_menu_selection = state.video_settings_menu_selection;
    snapshot.video_settings_target_window_size_index = state.video_settings_target_window_size_index;
    snapshot.video_settings_target_resolution_index = state.video_settings_target_resolution_index;
    snapshot.video_settings_target_fullscreen = state.video_settings_target_fullscreen;
    snapshot.rebuild_render_texture = state.rebuild_render_texture;
    snapshot.choosing_control_binding = state.choosing_control_binding;
    snapshot.show_entity_collision_boxes = state.show_entity_collision_boxes;
    snapshot.now = state.now;
    snapshot.time_since_last_update = state.time_since_last_update;
    snapshot.scene_frame = state.scene_frame;
    snapshot.frame = state.frame;
    snapshot.stage_frame = state.stage_frame;
    snapshot.menu_return_to = state.menu_return_to;
    snapshot.game_over = state.game_over;
    snapshot.pause = state.pause;
    snapshot.win = state.win;
    snapshot.next_stage = state.next_stage;
    snapshot.points = state.points;
    snapshot.deaths = state.deaths;
    snapshot.frame_pause = state.frame_pause;
    snapshot.debug_level = state.debug_level;
    snapshot.entity_manager = state.entity_manager;
    snapshot.stage = state.stage;
    snapshot.player_vid = state.player_vid;
    snapshot.mouse_trailer_vid = state.mouse_trailer_vid;
    snapshot.play_cam_pos = graphics.play_cam.pos;
    return snapshot;
}

void RestoreGameplaySnapshot(const GameplaySnapshot& snapshot, State& state, Graphics& graphics) {
    state.mode = snapshot.mode;
    state.settings = snapshot.settings;
    state.menu_inputs = snapshot.menu_inputs;
    state.menu_input_debounce_timers = snapshot.menu_input_debounce_timers;
    state.playing_inputs = snapshot.playing_inputs;
    state.title_menu_selection = snapshot.title_menu_selection;
    state.settings_menu_selection = snapshot.settings_menu_selection;
    state.video_settings_menu_selection = snapshot.video_settings_menu_selection;
    state.video_settings_target_window_size_index = snapshot.video_settings_target_window_size_index;
    state.video_settings_target_resolution_index = snapshot.video_settings_target_resolution_index;
    state.video_settings_target_fullscreen = snapshot.video_settings_target_fullscreen;
    state.rebuild_render_texture = snapshot.rebuild_render_texture;
    state.choosing_control_binding = snapshot.choosing_control_binding;
    state.show_entity_collision_boxes = snapshot.show_entity_collision_boxes;
    state.now = snapshot.now;
    state.time_since_last_update = snapshot.time_since_last_update;
    state.scene_frame = snapshot.scene_frame;
    state.frame = snapshot.frame;
    state.stage_frame = snapshot.stage_frame;
    state.menu_return_to = snapshot.menu_return_to;
    state.game_over = snapshot.game_over;
    state.pause = snapshot.pause;
    state.win = snapshot.win;
    state.next_stage = snapshot.next_stage;
    state.points = snapshot.points;
    state.deaths = snapshot.deaths;
    state.frame_pause = snapshot.frame_pause;
    state.debug_level = snapshot.debug_level;
    state.entity_manager = snapshot.entity_manager;
    state.special_effects.clear();
    state.stage = snapshot.stage;
    state.player_vid = snapshot.player_vid;
    state.mouse_trailer_vid = snapshot.mouse_trailer_vid;
    state.RebuildSid();
    graphics.play_cam.pos = snapshot.play_cam_pos;
}

void DrawDebugPlaybackControls(DebugPlayback& debug, State& state, Graphics& graphics) {
    DrawSimulationControls(debug, state, graphics);
    DrawLevelControls(debug, state, graphics);
}

void DrawDebugPlaybackInspector(DebugPlayback& debug, const State& state, const Graphics& graphics) {
    DrawEntityInspector(debug, state, graphics);
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
        ClampPlaybackIndex(debug);
        if (!debug.recorded_snapshots.empty()) {
            RestoreGameplaySnapshot(debug.recorded_snapshots[debug.playback_index], state, graphics);
        }
        graphics.debug_lock_play_camera = true;
        return;
    }

    AdvanceLiveSimulation(window, renderer, state, audio, graphics, debug, frame_dt);
}

} // namespace splonks

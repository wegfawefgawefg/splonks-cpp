#include "debug/playback_internal.hpp"

#include "entity/archetype.hpp"
#include "frame_data.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <type_traits>

namespace splonks::debug_playback_internal {

namespace {

constexpr std::uint32_t kRecordingMagic = 0x53504C52U;
constexpr std::uint32_t kRecordingVersion = 36;

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
        out.write(
            reinterpret_cast<const char*>(values.data()),
            static_cast<std::streamsize>(sizeof(T) * values.size())
        );
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
        in.read(
            reinterpret_cast<char*>(values.data()),
            static_cast<std::streamsize>(sizeof(T) * values.size())
        );
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
    WritePod(out, settings.ui.icon_scale);
    WritePod(out, settings.ui.status_icon_scale);
    WritePod(out, settings.ui.tool_slot_scale);
    WritePod(out, settings.ui.tool_icon_scale);
    WritePod(out, settings.post_process.effect);
    WritePod(out, settings.post_process.terrain_lighting);
    WritePod(out, settings.post_process.terrain_face_shading);
    WritePod(out, settings.post_process.terrain_face_enclosed_stage_bounds);
    WritePod(out, settings.post_process.terrain_seam_ao);
    WritePod(out, settings.post_process.terrain_exposure_lighting);
    WritePod(out, settings.post_process.backwall_lighting);
    WritePod(out, settings.post_process.terrain_face_top_highlight);
    WritePod(out, settings.post_process.terrain_face_side_shade);
    WritePod(out, settings.post_process.terrain_face_bottom_shade);
    WritePod(out, settings.post_process.terrain_face_band_size);
    WritePod(out, settings.post_process.terrain_face_gradient_softness);
    WritePod(out, settings.post_process.terrain_face_corner_rounding);
    WritePod(out, settings.post_process.terrain_seam_ao_amount);
    WritePod(out, settings.post_process.terrain_seam_ao_size);
    WritePod(out, settings.post_process.terrain_exposure_amount);
    WritePod(out, settings.post_process.terrain_exposure_min_brightness);
    WritePod(out, settings.post_process.terrain_exposure_max_brightness);
    WritePod(out, settings.post_process.terrain_exposure_diagonal_weight);
    WritePod(out, settings.post_process.terrain_exposure_smoothing);
    WritePod(out, settings.post_process.backwall_brightness);
    WritePod(out, settings.post_process.backwall_min_brightness);
    WritePod(out, settings.post_process.backwall_max_brightness);
    WritePod(out, settings.post_process.backwall_smoothing);
    WritePod(out, settings.post_process.crt_scanline_amount);
    WritePod(out, settings.post_process.crt_scanline_edge_start);
    WritePod(out, settings.post_process.crt_scanline_edge_falloff);
    WritePod(out, settings.post_process.crt_scanline_edge_strength);
    WritePod(out, settings.post_process.crt_zoom);
    WritePod(out, settings.post_process.crt_warp_amount);
    WritePod(out, settings.post_process.crt_vignette_amount);
    WritePod(out, settings.post_process.crt_vignette_intensity);
    WritePod(out, settings.post_process.crt_grille_amount);
    WritePod(out, settings.post_process.crt_brightness_boost);
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
        !ReadPod(in, settings.controls.shoot) ||
        !ReadPod(in, settings.ui.icon_scale) ||
        !ReadPod(in, settings.ui.status_icon_scale) ||
        !ReadPod(in, settings.ui.tool_slot_scale) ||
        !ReadPod(in, settings.ui.tool_icon_scale) ||
        !ReadPod(in, settings.post_process.effect) ||
        !ReadPod(in, settings.post_process.terrain_lighting) ||
        !ReadPod(in, settings.post_process.terrain_face_shading) ||
        !ReadPod(in, settings.post_process.terrain_face_enclosed_stage_bounds) ||
        !ReadPod(in, settings.post_process.terrain_seam_ao) ||
        !ReadPod(in, settings.post_process.terrain_exposure_lighting) ||
        !ReadPod(in, settings.post_process.backwall_lighting) ||
        !ReadPod(in, settings.post_process.terrain_face_top_highlight) ||
        !ReadPod(in, settings.post_process.terrain_face_side_shade) ||
        !ReadPod(in, settings.post_process.terrain_face_bottom_shade) ||
        !ReadPod(in, settings.post_process.terrain_face_band_size) ||
        !ReadPod(in, settings.post_process.terrain_face_gradient_softness) ||
        !ReadPod(in, settings.post_process.terrain_face_corner_rounding) ||
        !ReadPod(in, settings.post_process.terrain_seam_ao_amount) ||
        !ReadPod(in, settings.post_process.terrain_seam_ao_size) ||
        !ReadPod(in, settings.post_process.terrain_exposure_amount) ||
        !ReadPod(in, settings.post_process.terrain_exposure_min_brightness) ||
        !ReadPod(in, settings.post_process.terrain_exposure_max_brightness) ||
        !ReadPod(in, settings.post_process.terrain_exposure_diagonal_weight) ||
        !ReadPod(in, settings.post_process.terrain_exposure_smoothing) ||
        !ReadPod(in, settings.post_process.backwall_brightness) ||
        !ReadPod(in, settings.post_process.backwall_min_brightness) ||
        !ReadPod(in, settings.post_process.backwall_max_brightness) ||
        !ReadPod(in, settings.post_process.backwall_smoothing) ||
        !ReadPod(in, settings.post_process.crt_scanline_amount) ||
        !ReadPod(in, settings.post_process.crt_scanline_edge_start) ||
        !ReadPod(in, settings.post_process.crt_scanline_edge_falloff) ||
        !ReadPod(in, settings.post_process.crt_scanline_edge_strength) ||
        !ReadPod(in, settings.post_process.crt_zoom) ||
        !ReadPod(in, settings.post_process.crt_warp_amount) ||
        !ReadPod(in, settings.post_process.crt_vignette_amount) ||
        !ReadPod(in, settings.post_process.crt_vignette_intensity) ||
        !ReadPod(in, settings.post_process.crt_grille_amount) ||
        !ReadPod(in, settings.post_process.crt_brightness_boost)) {
        return false;
    }
    return true;
}

void WriteStage(std::ostream& out, const Stage& stage) {
    WritePod(out, stage.stage_type);
    WritePod(out, stage.gravity);
    WritePod(out, stage.border.left.tile);
    WritePod(out, stage.border.right.tile);
    WritePod(out, stage.border.top.tile);
    WritePod(out, stage.border.bottom.tile);
    WritePod(out, stage.border.wrap_x);
    WritePod(out, stage.border.wrap_y);
    WriteOptionalPod(out, stage.border.void_death_y);
    WritePod(out, stage.camera_clamp_enabled);
    WritePod(out, stage.camera_clamp_margin);
    WritePod(out, stage.wrap_transform_active);
    WritePod(out, stage.wrap_padding_chunks);
    WritePod(out, stage.wrap_core_origin_tiles);
    WritePod(out, stage.wrap_core_size_tiles);
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
        !ReadPod(in, stage.border.left.tile) ||
        !ReadPod(in, stage.border.right.tile) ||
        !ReadPod(in, stage.border.top.tile) ||
        !ReadPod(in, stage.border.bottom.tile) ||
        !ReadPod(in, stage.border.wrap_x) ||
        !ReadPod(in, stage.border.wrap_y) ||
        !ReadOptionalPod(in, stage.border.void_death_y) ||
        !ReadPod(in, stage.camera_clamp_enabled) ||
        !ReadPod(in, stage.camera_clamp_margin) ||
        !ReadPod(in, stage.wrap_transform_active) ||
        !ReadPod(in, stage.wrap_padding_chunks) ||
        !ReadPod(in, stage.wrap_core_origin_tiles) ||
        !ReadPod(in, stage.wrap_core_size_tiles)) {
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
    WritePod(out, snapshot.menu_input_snapshot);
    WritePod(out, snapshot.previous_menu_input_snapshot);
    WritePod(out, snapshot.menu_input_debounce_timers);
    WritePod(out, snapshot.playing_inputs);
    WritePod(out, snapshot.immediate_playing_inputs);
    WritePod(out, snapshot.playing_input_snapshot);
    WritePod(out, snapshot.previous_playing_input_snapshot);
    WritePod(out, snapshot.previous_immediate_playing_input_snapshot);
    WritePod(out, snapshot.title_menu_selection);
    WritePod(out, snapshot.settings_menu_selection);
    WritePod(out, snapshot.video_settings_menu_selection);
    WritePod(out, snapshot.ui_settings_menu_selection);
    WritePod(out, snapshot.post_fx_settings_menu_selection);
    WritePod(out, snapshot.lighting_settings_menu_selection);
    WriteOptionalPod(out, snapshot.video_settings_target_window_size_index);
    WriteOptionalPod(out, snapshot.video_settings_target_resolution_index);
    WriteOptionalPod(out, snapshot.video_settings_target_fullscreen);
    WritePod(out, snapshot.rebuild_render_texture);
    WritePod(out, snapshot.choosing_control_binding);
    WritePod(out, snapshot.show_entity_collision_boxes);
    WritePod(out, snapshot.show_entity_ids);
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
    WriteOptionalPod(out, snapshot.controlled_entity_vid);
    WriteOptionalPod(out, snapshot.mouse_trailer_vid);
    WriteVectorPod(out, snapshot.entity_tool_states);
    WritePod(out, snapshot.play_cam_pos);
}

bool ReadSnapshot(std::istream& in, GameplaySnapshot& snapshot) {
    return ReadPod(in, snapshot.mode) &&
           ReadSettings(in, snapshot.settings) &&
           ReadPod(in, snapshot.menu_inputs) &&
           ReadPod(in, snapshot.menu_input_snapshot) &&
           ReadPod(in, snapshot.previous_menu_input_snapshot) &&
           ReadPod(in, snapshot.menu_input_debounce_timers) &&
           ReadPod(in, snapshot.playing_inputs) &&
           ReadPod(in, snapshot.immediate_playing_inputs) &&
           ReadPod(in, snapshot.playing_input_snapshot) &&
           ReadPod(in, snapshot.previous_playing_input_snapshot) &&
           ReadPod(in, snapshot.previous_immediate_playing_input_snapshot) &&
           ReadPod(in, snapshot.title_menu_selection) &&
           ReadPod(in, snapshot.settings_menu_selection) &&
           ReadPod(in, snapshot.video_settings_menu_selection) &&
           ReadPod(in, snapshot.ui_settings_menu_selection) &&
           ReadPod(in, snapshot.post_fx_settings_menu_selection) &&
           ReadPod(in, snapshot.lighting_settings_menu_selection) &&
           ReadOptionalPod(in, snapshot.video_settings_target_window_size_index) &&
           ReadOptionalPod(in, snapshot.video_settings_target_resolution_index) &&
           ReadOptionalPod(in, snapshot.video_settings_target_fullscreen) &&
           ReadPod(in, snapshot.rebuild_render_texture) &&
           ReadPod(in, snapshot.choosing_control_binding) &&
           ReadPod(in, snapshot.show_entity_collision_boxes) &&
           ReadPod(in, snapshot.show_entity_ids) &&
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
           ReadOptionalPod(in, snapshot.controlled_entity_vid) &&
           ReadOptionalPod(in, snapshot.mouse_trailer_vid) &&
           ReadVectorPod(in, snapshot.entity_tool_states) &&
           ReadPod(in, snapshot.play_cam_pos);
}

} // namespace

const char* ModeToString(Mode mode) {
    switch (mode) {
    case Mode::Title:
        return "Title";
    case Mode::Settings:
        return "Settings";
    case Mode::VideoSettings:
        return "VideoSettings";
    case Mode::UiSettings:
        return "UiSettings";
    case Mode::PostFxSettings:
        return "PostFxSettings";
    case Mode::LightingSettings:
        return "LightingSettings";
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
    case DebugLevelKind::Cave1:
        return "Cave1";
    case DebugLevelKind::HangTest:
        return "HangTest";
    case DebugLevelKind::StompTest:
        return "StompTest";
    case DebugLevelKind::BorderTest:
        return "BorderTest";
    }
    return "Unknown";
}

const char* EntityTypeToString(EntityType type) {
    return GetEntityTypeName(type);
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

const char* ConditionToString(EntityCondition condition) {
    switch (condition) {
    case EntityCondition::Normal:
        return "Normal";
    case EntityCondition::Dead:
        return "Dead";
    case EntityCondition::Stunned:
        return "Stunned";
    }
    return "Unknown";
}

const char* AiStateToString(EntityAiState ai_state) {
    switch (ai_state) {
    case EntityAiState::Idle:
        return "Idle";
    case EntityAiState::Pursuing:
        return "Pursuing";
    case EntityAiState::Returning:
        return "Returning";
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
    debug.playback_index =
        debug.recorded_snapshots.empty() ? 0 : debug.recorded_snapshots.size() - 1;

    if (status_out != nullptr) {
        char buffer[128];
        std::snprintf(buffer, sizeof(buffer), "Loaded %u snapshots.", count);
        *status_out = buffer;
    }
    return true;
}

bool ExportRecordingToTextFile(
    const DebugPlayback& debug,
    const Graphics& graphics,
    std::string* status_out
) {
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

    for (std::size_t snapshot_index = 0; snapshot_index < debug.recorded_snapshots.size();
         ++snapshot_index) {
        const GameplaySnapshot& snapshot = debug.recorded_snapshots[snapshot_index];
        out << "snapshot " << snapshot_index << "\n";
        out << "  mode: " << ModeToString(snapshot.mode) << "\n";
        out << "  scene_frame: " << snapshot.scene_frame << "\n";
        out << "  frame: " << snapshot.frame << "\n";
        out << "  stage_frame: " << snapshot.stage_frame << "\n";
        out << "  stage_type: " << static_cast<int>(snapshot.stage.stage_type) << "\n";
        out << "  points: " << snapshot.points << "\n";
        out << "  deaths: " << snapshot.deaths << "\n";
        out << "  play_cam_pos: (" << snapshot.play_cam_pos.x << ", " << snapshot.play_cam_pos.y
            << ")\n";

        std::size_t active_count = 0;
        for (const Entity& entity : snapshot.entity_manager.entities) {
            if (entity.active) {
                active_count += 1;
            }
        }
        out << "  active_entities: " << active_count << "\n";

        for (std::size_t entity_id = 0; entity_id < snapshot.entity_manager.entities.size();
             ++entity_id) {
            const Entity& entity = snapshot.entity_manager.entities[entity_id];
            if (!entity.active) {
                continue;
            }

            out << "  entity " << entity_id << "\n";
            out << "    type: " << EntityTypeToString(entity.type_) << "\n";
            out << "    condition: " << ConditionToString(entity.condition) << "\n";
            out << "    ai_state: " << AiStateToString(entity.ai_state) << "\n";
            out << "    facing: " << LeftOrRightToString(entity.facing) << "\n";
            out << "    grounded: " << (entity.grounded ? "true" : "false") << "\n";
            out << "    climbing: " << (entity.IsClimbing() ? "true" : "false") << "\n";
            out << "    holding: " << (entity.holding ? "true" : "false") << "\n";
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
        std::snprintf(
            buffer,
            sizeof(buffer),
            "Exported %zu snapshots as text.",
            debug.recorded_snapshots.size()
        );
        *status_out = buffer;
    }
    return true;
}

} // namespace splonks::debug_playback_internal

namespace splonks {

bool ConvertRecordingFileToText(
    const std::string& input_path,
    const std::string& output_path,
    const FrameDataDb& frame_data_db,
    std::string* status_out
) {
    DebugPlayback debug = DebugPlayback::New();
    std::strncpy(debug.file_path.data(), input_path.c_str(), debug.file_path.size() - 1);
    debug.file_path[debug.file_path.size() - 1] = '\0';
    if (!debug_playback_internal::LoadRecordingFromFile(debug, status_out)) {
        return false;
    }

    Graphics graphics{};
    graphics.frame_data_db = frame_data_db;
    std::strncpy(debug.file_path.data(), output_path.c_str(), debug.file_path.size() - 1);
    debug.file_path[debug.file_path.size() - 1] = '\0';
    return debug_playback_internal::ExportRecordingToTextFile(debug, graphics, status_out);
}

GameplaySnapshot MakeGameplaySnapshot(const State& state, const Graphics& graphics) {
    GameplaySnapshot snapshot;
    snapshot.mode = state.mode;
    snapshot.settings = state.settings;
    snapshot.menu_inputs = state.menu_inputs;
    snapshot.menu_input_snapshot = state.menu_input_snapshot;
    snapshot.previous_menu_input_snapshot = state.previous_menu_input_snapshot;
    snapshot.menu_input_debounce_timers = state.menu_input_debounce_timers;
    snapshot.playing_inputs = state.playing_inputs;
    snapshot.immediate_playing_inputs = state.immediate_playing_inputs;
    snapshot.playing_input_snapshot = state.playing_input_snapshot;
    snapshot.previous_playing_input_snapshot = state.previous_playing_input_snapshot;
    snapshot.previous_immediate_playing_input_snapshot =
        state.previous_immediate_playing_input_snapshot;
    snapshot.title_menu_selection = state.title_menu_selection;
    snapshot.settings_menu_selection = state.settings_menu_selection;
    snapshot.video_settings_menu_selection = state.video_settings_menu_selection;
    snapshot.ui_settings_menu_selection = state.ui_settings_menu_selection;
    snapshot.post_fx_settings_menu_selection = state.post_fx_settings_menu_selection;
    snapshot.lighting_settings_menu_selection = state.lighting_settings_menu_selection;
    snapshot.video_settings_target_window_size_index = state.video_settings_target_window_size_index;
    snapshot.video_settings_target_resolution_index = state.video_settings_target_resolution_index;
    snapshot.video_settings_target_fullscreen = state.video_settings_target_fullscreen;
    snapshot.rebuild_render_texture = state.rebuild_render_texture;
    snapshot.choosing_control_binding = state.choosing_control_binding;
    snapshot.show_entity_collision_boxes = state.show_entity_collision_boxes;
    snapshot.show_entity_ids = state.show_entity_ids;
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
    snapshot.controlled_entity_vid = state.controlled_entity_vid;
    snapshot.mouse_trailer_vid = state.mouse_trailer_vid;
    snapshot.entity_tool_states = state.entity_tool_states;
    snapshot.play_cam_pos = graphics.play_cam.pos;
    return snapshot;
}

void RestoreGameplaySnapshot(const GameplaySnapshot& snapshot, State& state, Graphics& graphics) {
    state.mode = snapshot.mode;
    state.settings = snapshot.settings;
    state.menu_inputs = snapshot.menu_inputs;
    state.menu_input_snapshot = snapshot.menu_input_snapshot;
    state.previous_menu_input_snapshot = snapshot.previous_menu_input_snapshot;
    state.menu_input_debounce_timers = snapshot.menu_input_debounce_timers;
    state.playing_inputs = snapshot.playing_inputs;
    state.immediate_playing_inputs = snapshot.immediate_playing_inputs;
    state.playing_input_snapshot = snapshot.playing_input_snapshot;
    state.previous_playing_input_snapshot = snapshot.previous_playing_input_snapshot;
    state.previous_immediate_playing_input_snapshot =
        snapshot.previous_immediate_playing_input_snapshot;
    state.title_menu_selection = snapshot.title_menu_selection;
    state.settings_menu_selection = snapshot.settings_menu_selection;
    state.video_settings_menu_selection = snapshot.video_settings_menu_selection;
    state.ui_settings_menu_selection = snapshot.ui_settings_menu_selection;
    state.post_fx_settings_menu_selection = snapshot.post_fx_settings_menu_selection;
    state.lighting_settings_menu_selection = snapshot.lighting_settings_menu_selection;
    state.video_settings_target_window_size_index = snapshot.video_settings_target_window_size_index;
    state.video_settings_target_resolution_index = snapshot.video_settings_target_resolution_index;
    state.video_settings_target_fullscreen = snapshot.video_settings_target_fullscreen;
    state.rebuild_render_texture = snapshot.rebuild_render_texture;
    state.choosing_control_binding = snapshot.choosing_control_binding;
    state.show_entity_collision_boxes = snapshot.show_entity_collision_boxes;
    state.show_entity_ids = snapshot.show_entity_ids;
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
    state.controlled_entity_vid = snapshot.controlled_entity_vid;
    state.mouse_trailer_vid = snapshot.mouse_trailer_vid;
    state.entity_tool_states = snapshot.entity_tool_states;
    state.RebuildSid(graphics);
    graphics.play_cam.pos = snapshot.play_cam_pos;
}

} // namespace splonks

#include "debug/playback_internal.hpp"

#include <cstdio>
#include <fstream>
#include <type_traits>

namespace splonks::debug_playback_internal {

namespace {

constexpr std::uint32_t kRecordingMagic = 0x53504C52U;
constexpr std::uint32_t kRecordingVersion = 45;

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

template <typename T>
void WriteOptionalVectorPod(std::ostream& out, const std::optional<std::vector<T>>& values) {
    const bool has_value = values.has_value();
    WritePod(out, has_value);
    if (has_value) {
        WriteVectorPod(out, *values);
    }
}

template <typename T>
bool ReadOptionalVectorPod(std::istream& in, std::optional<std::vector<T>>& values) {
    bool has_value = false;
    if (!ReadPod(in, has_value)) {
        return false;
    }
    if (!has_value) {
        values.reset();
        return true;
    }
    values.emplace();
    return ReadVectorPod(in, *values);
}

void WriteEntity(std::ostream& out, const Entity& entity) {
    WritePod(out, entity.active);
    WritePod(out, entity.marked_for_destruction);
    WritePod(out, entity.type_);
    WritePod(out, entity.vid);
    WritePod(out, entity.was_horizontally_controlled_this_frame);
    WritePod(out, entity.has_physics);
    WritePod(out, entity.can_collide);
    WritePod(out, entity.can_be_hit);
    WritePod(out, entity.stone);
    WritePod(out, entity.wanted);
    WritePod(out, entity.crusher_pusher);
    WritePod(out, entity.can_stomp);
    WritePod(out, entity.can_be_stomped);
    WritePod(out, entity.can_go_on_back);
    WritePod(out, entity.grounded);
    WritePod(out, entity.coyote_time);
    WritePod(out, entity.stun_timer);
    WritePod(out, entity.stun_recovers_on_ground);
    WritePod(out, entity.stun_recovers_while_held);
    WritePod(out, entity.can_be_picked_up);
    WritePod(out, entity.can_only_be_picked_up_if_dead_or_stunned);
    WritePod(out, entity.impassable);
    WritePod(out, entity.fall_distance);
    WritePod(out, entity.pos);
    WritePod(out, entity.vel);
    WritePod(out, entity.acc);
    WritePod(out, entity.max_speed);
    WritePod(out, entity.size);
    WritePod(out, entity.dist_traveled_this_frame);
    WritePod(out, entity.facing);
    WritePod(out, entity.vertical_flip);
    WritePod(out, entity.draw_layer);
    WritePod(out, entity.render_enabled);
    WritePod(out, entity.frame_data_animator);
    WritePod(out, entity.jump_delay_frame_count);
    WritePod(out, entity.jumped_this_frame);
    WriteOptionalPod(out, entity.hang_side);
    WritePod(out, entity.can_hang_ledge);
    WritePod(out, entity.can_hang_wall);
    WritePod(out, entity.hang_count);
    WritePod(out, entity.holding);
    WritePod(out, entity.passive_item_flags);
    WriteOptionalPod(out, entity.passive_item);
    WritePod(out, entity.money);
    WritePod(out, entity.buyable);
    WritePod(out, entity.bombs);
    WritePod(out, entity.ropes);
    WriteOptionalPod(out, entity.back_vid);
    WritePod(out, entity.attachment_mode);
    WritePod(out, entity.use_state);
    WritePod(out, entity.travel_sound_countdown);
    WritePod(out, entity.travel_sound);
    WritePod(out, entity.condition);
    WritePod(out, entity.last_condition);
    WritePod(out, entity.ai_state);
    WritePod(out, entity.last_ai_state);
    WritePod(out, entity.movement_flags);
    WritePod(out, entity.health);
    WritePod(out, entity.hurt_on_contact);
    WritePod(out, entity.vanish_on_death);
    WritePod(out, entity.has_ground_friction);
    WriteOptionalPod(out, entity.damage_animation);
    WriteOptionalPod(out, entity.damage_sound);
    WriteOptionalPod(out, entity.collide_sound);
    WriteOptionalPod(out, entity.death_sound_effect);
    WritePod(out, entity.on_death);
    WritePod(out, entity.on_damage);
    WritePod(out, entity.on_use);
    WritePod(out, entity.on_area_enter);
    WritePod(out, entity.on_area_exit);
    WritePod(out, entity.on_area_tile_changed);
    WritePod(out, entity.step_logic);
    WritePod(out, entity.step_physics);
    WriteOptionalPod(out, entity.transition_target);
    WritePod(out, entity.attack_weight);
    WritePod(out, entity.weight);
    WritePod(out, entity.bomb_throw_delay_countdown);
    WritePod(out, entity.rope_throw_delay_countdown);
    WritePod(out, entity.attack_delay_countdown);
    WritePod(out, entity.equip_delay_countdown);
    WriteOptionalPod(out, entity.thrown_by);
    WritePod(out, entity.thrown_immunity_timer);
    WritePod(out, entity.projectile_contact_damage_type);
    WritePod(out, entity.projectile_contact_damage_amount);
    WritePod(out, entity.projectile_contact_timer);
    WritePod(out, entity.collided);
    WritePod(out, entity.collided_last_frame);
    WritePod(out, entity.contact_sound_cooldown);
    WritePod(out, entity.damage_vulnerability);
    WritePod(out, entity.can_be_stunned);
    WritePod(out, entity.point_a);
    WritePod(out, entity.point_b);
    WritePod(out, entity.point_c);
    WritePod(out, entity.point_d);
    WritePod(out, entity.point_label_a);
    WritePod(out, entity.point_label_b);
    WritePod(out, entity.point_label_c);
    WritePod(out, entity.point_label_d);
    WriteOptionalPod(out, entity.holding_vid);
    WriteOptionalPod(out, entity.held_by_vid);
    WritePod(out, entity.holding_timer);
    WriteOptionalPod(out, entity.entity_a);
    WriteOptionalPod(out, entity.entity_b);
    WriteOptionalPod(out, entity.entity_c);
    WriteOptionalPod(out, entity.entity_d);
    WriteOptionalVectorPod(out, entity.child_vids);
    WriteOptionalVectorPod(out, entity.inside_vids);
    WritePod(out, entity.entity_label_a);
    WritePod(out, entity.entity_label_b);
    WritePod(out, entity.entity_label_c);
    WritePod(out, entity.entity_label_d);
    WritePod(out, entity.alignment);
    WritePod(out, entity.counter_a);
    WritePod(out, entity.counter_b);
    WritePod(out, entity.counter_c);
    WritePod(out, entity.counter_d);
    WritePod(out, entity.threshold_a);
    WritePod(out, entity.threshold_b);
    WritePod(out, entity.threshold_c);
    WritePod(out, entity.threshold_d);
}

bool ReadEntity(std::istream& in, Entity& entity) {
    return ReadPod(in, entity.active) &&
           ReadPod(in, entity.marked_for_destruction) &&
           ReadPod(in, entity.type_) &&
           ReadPod(in, entity.vid) &&
           ReadPod(in, entity.was_horizontally_controlled_this_frame) &&
           ReadPod(in, entity.has_physics) &&
           ReadPod(in, entity.can_collide) &&
           ReadPod(in, entity.can_be_hit) &&
           ReadPod(in, entity.stone) &&
           ReadPod(in, entity.wanted) &&
           ReadPod(in, entity.crusher_pusher) &&
           ReadPod(in, entity.can_stomp) &&
           ReadPod(in, entity.can_be_stomped) &&
           ReadPod(in, entity.can_go_on_back) &&
           ReadPod(in, entity.grounded) &&
           ReadPod(in, entity.coyote_time) &&
           ReadPod(in, entity.stun_timer) &&
           ReadPod(in, entity.stun_recovers_on_ground) &&
           ReadPod(in, entity.stun_recovers_while_held) &&
           ReadPod(in, entity.can_be_picked_up) &&
           ReadPod(in, entity.can_only_be_picked_up_if_dead_or_stunned) &&
           ReadPod(in, entity.impassable) &&
           ReadPod(in, entity.fall_distance) &&
           ReadPod(in, entity.pos) &&
           ReadPod(in, entity.vel) &&
           ReadPod(in, entity.acc) &&
           ReadPod(in, entity.max_speed) &&
           ReadPod(in, entity.size) &&
           ReadPod(in, entity.dist_traveled_this_frame) &&
           ReadPod(in, entity.facing) &&
           ReadPod(in, entity.vertical_flip) &&
           ReadPod(in, entity.draw_layer) &&
           ReadPod(in, entity.render_enabled) &&
           ReadPod(in, entity.frame_data_animator) &&
           ReadPod(in, entity.jump_delay_frame_count) &&
           ReadPod(in, entity.jumped_this_frame) &&
           ReadOptionalPod(in, entity.hang_side) &&
           ReadPod(in, entity.can_hang_ledge) &&
           ReadPod(in, entity.can_hang_wall) &&
           ReadPod(in, entity.hang_count) &&
           ReadPod(in, entity.holding) &&
           ReadPod(in, entity.passive_item_flags) &&
           ReadOptionalPod(in, entity.passive_item) &&
           ReadPod(in, entity.money) &&
           ReadPod(in, entity.buyable) &&
           ReadPod(in, entity.bombs) &&
           ReadPod(in, entity.ropes) &&
           ReadOptionalPod(in, entity.back_vid) &&
           ReadPod(in, entity.attachment_mode) &&
           ReadPod(in, entity.use_state) &&
           ReadPod(in, entity.travel_sound_countdown) &&
           ReadPod(in, entity.travel_sound) &&
           ReadPod(in, entity.condition) &&
           ReadPod(in, entity.last_condition) &&
           ReadPod(in, entity.ai_state) &&
           ReadPod(in, entity.last_ai_state) &&
           ReadPod(in, entity.movement_flags) &&
           ReadPod(in, entity.health) &&
           ReadPod(in, entity.hurt_on_contact) &&
           ReadPod(in, entity.vanish_on_death) &&
           ReadPod(in, entity.has_ground_friction) &&
           ReadOptionalPod(in, entity.damage_animation) &&
           ReadOptionalPod(in, entity.damage_sound) &&
           ReadOptionalPod(in, entity.collide_sound) &&
           ReadOptionalPod(in, entity.death_sound_effect) &&
           ReadPod(in, entity.on_death) &&
           ReadPod(in, entity.on_damage) &&
           ReadPod(in, entity.on_use) &&
           ReadPod(in, entity.on_area_enter) &&
           ReadPod(in, entity.on_area_exit) &&
           ReadPod(in, entity.on_area_tile_changed) &&
           ReadPod(in, entity.step_logic) &&
           ReadPod(in, entity.step_physics) &&
           ReadOptionalPod(in, entity.transition_target) &&
           ReadPod(in, entity.attack_weight) &&
           ReadPod(in, entity.weight) &&
           ReadPod(in, entity.bomb_throw_delay_countdown) &&
           ReadPod(in, entity.rope_throw_delay_countdown) &&
           ReadPod(in, entity.attack_delay_countdown) &&
           ReadPod(in, entity.equip_delay_countdown) &&
           ReadOptionalPod(in, entity.thrown_by) &&
           ReadPod(in, entity.thrown_immunity_timer) &&
           ReadPod(in, entity.projectile_contact_damage_type) &&
           ReadPod(in, entity.projectile_contact_damage_amount) &&
           ReadPod(in, entity.projectile_contact_timer) &&
           ReadPod(in, entity.collided) &&
           ReadPod(in, entity.collided_last_frame) &&
           ReadPod(in, entity.contact_sound_cooldown) &&
           ReadPod(in, entity.damage_vulnerability) &&
           ReadPod(in, entity.can_be_stunned) &&
           ReadPod(in, entity.point_a) &&
           ReadPod(in, entity.point_b) &&
           ReadPod(in, entity.point_c) &&
           ReadPod(in, entity.point_d) &&
           ReadPod(in, entity.point_label_a) &&
           ReadPod(in, entity.point_label_b) &&
           ReadPod(in, entity.point_label_c) &&
           ReadPod(in, entity.point_label_d) &&
           ReadOptionalPod(in, entity.holding_vid) &&
           ReadOptionalPod(in, entity.held_by_vid) &&
           ReadPod(in, entity.holding_timer) &&
           ReadOptionalPod(in, entity.entity_a) &&
           ReadOptionalPod(in, entity.entity_b) &&
           ReadOptionalPod(in, entity.entity_c) &&
           ReadOptionalPod(in, entity.entity_d) &&
           ReadOptionalVectorPod(in, entity.child_vids) &&
           ReadOptionalVectorPod(in, entity.inside_vids) &&
           ReadPod(in, entity.entity_label_a) &&
           ReadPod(in, entity.entity_label_b) &&
           ReadPod(in, entity.entity_label_c) &&
           ReadPod(in, entity.entity_label_d) &&
           ReadPod(in, entity.alignment) &&
           ReadPod(in, entity.counter_a) &&
           ReadPod(in, entity.counter_b) &&
           ReadPod(in, entity.counter_c) &&
           ReadPod(in, entity.counter_d) &&
           ReadPod(in, entity.threshold_a) &&
           ReadPod(in, entity.threshold_b) &&
           ReadPod(in, entity.threshold_c) &&
           ReadPod(in, entity.threshold_d);
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

    const std::uint32_t backwall_rows = static_cast<std::uint32_t>(stage.backwall_tiles.size());
    WritePod(out, backwall_rows);
    for (const std::vector<Tile>& row : stage.backwall_tiles) {
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

    std::uint32_t backwall_rows = 0;
    if (!ReadPod(in, backwall_rows)) {
        return false;
    }
    stage.backwall_tiles.resize(backwall_rows);
    for (std::uint32_t i = 0; i < backwall_rows; ++i) {
        if (!ReadVectorPod(in, stage.backwall_tiles[i])) {
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
    const std::uint32_t entity_count = static_cast<std::uint32_t>(entity_manager.entities.size());
    WritePod(out, entity_count);
    for (const Entity& entity : entity_manager.entities) {
        WriteEntity(out, entity);
    }
    WriteVectorPod(out, entity_manager.available_ids);
}

bool ReadEntityManager(std::istream& in, EntityManager& entity_manager) {
    std::uint32_t entity_count = 0;
    if (!ReadPod(in, entity_count)) {
        return false;
    }

    entity_manager.entities.resize(entity_count);
    for (std::uint32_t i = 0; i < entity_count; ++i) {
        if (!ReadEntity(in, entity_manager.entities[i])) {
            return false;
        }
    }

    return ReadVectorPod(in, entity_manager.available_ids);
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
    WritePod(out, snapshot.debug_overlay);
    WritePod(out, snapshot.now);
    WritePod(out, snapshot.time_since_last_update);
    WritePod(out, snapshot.scene_frame);
    WritePod(out, snapshot.frame);
    WritePod(out, snapshot.stage_frame);
    WritePod(out, snapshot.menu_return_to);
    WritePod(out, snapshot.game_over);
    WritePod(out, snapshot.pause);
    WritePod(out, snapshot.win);
    WritePod(out, snapshot.respawn_target);
    WriteOptionalPod(out, snapshot.pending_stage_transition);
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
           ReadPod(in, snapshot.debug_overlay) &&
           ReadPod(in, snapshot.now) &&
           ReadPod(in, snapshot.time_since_last_update) &&
           ReadPod(in, snapshot.scene_frame) &&
           ReadPod(in, snapshot.frame) &&
           ReadPod(in, snapshot.stage_frame) &&
           ReadPod(in, snapshot.menu_return_to) &&
           ReadPod(in, snapshot.game_over) &&
           ReadPod(in, snapshot.pause) &&
           ReadPod(in, snapshot.win) &&
           ReadPod(in, snapshot.respawn_target) &&
           ReadOptionalPod(in, snapshot.pending_stage_transition) &&
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

} // namespace splonks::debug_playback_internal

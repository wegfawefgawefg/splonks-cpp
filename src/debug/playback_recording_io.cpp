#include "debug/playback_internal.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <type_traits>

namespace splonks::debug_playback_internal {

namespace {

constexpr std::uint32_t kRecordingMagic = 0x53504C52U;
constexpr std::uint32_t kRecordingVersion = 71;

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

void WriteString(std::ostream& out, const std::string& value) {
    const std::uint32_t count = static_cast<std::uint32_t>(value.size());
    WritePod(out, count);
    if (count > 0) {
        out.write(value.data(), static_cast<std::streamsize>(count));
    }
}

bool ReadString(std::istream& in, std::string& value) {
    std::uint32_t count = 0;
    if (!ReadPod(in, count)) {
        return false;
    }
    value.resize(count);
    if (count > 0) {
        in.read(value.data(), static_cast<std::streamsize>(count));
    }
    return in.good();
}

void WriteEntEffects(std::ostream& out, const BoxedEntEffects& effects_box) {
    const EntEffects* const effects = effects_box.get();
    const std::uint8_t count = effects != nullptr ? effects->count : 0;
    WritePod(out, count);
    for (std::uint8_t i = 0; i < count; ++i) {
        WritePod(out, effects->effects[i]);
    }
}

bool ReadEntEffects(std::istream& in, BoxedEntEffects& effects_box) {
    std::uint8_t count = 0;
    if (!ReadPod(in, count)) {
        return false;
    }
    if (count > kMaxEntEffects) {
        return false;
    }
    if (count == 0) {
        effects_box.reset();
        return true;
    }
    EntEffects& effects = effects_box.emplace();
    effects.count = count;
    for (std::uint8_t i = 0; i < count; ++i) {
        if (!ReadPod(in, effects.effects[i])) {
            return false;
        }
    }
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

void WriteEnt(std::ostream& out, const Ent& ent) {
    WritePod(out, ent.active);
    WritePod(out, ent.marked_for_destruction);
    WritePod(out, ent.type_);
    WritePod(out, ent.vid);
    WritePod(out, ent.has_physics);
    WritePod(out, ent.can_collide);
    WritePod(out, ent.can_be_hit);
    WritePod(out, ent.can_receive_proj_contact);
    WritePod(out, ent.stone);
    WritePod(out, ent.wanted);
    WritePod(out, ent.crusher_pusher);
    WritePod(out, ent.pushable);
    WritePod(out, ent.can_stomp);
    WritePod(out, ent.can_be_stomped);
    WritePod(out, ent.can_collect_pickups);
    WritePod(out, ent.can_go_on_back);
    WritePod(out, ent.grounded);
    WritePod(out, ent.shake);
    WritePod(out, ent.rotation);
    WritePod(out, ent.alpha);
    WritePod(out, ent.coyote_time);
    WritePod(out, ent.stun_timer);
    WritePod(out, ent.stun_recovers_on_ground);
    WritePod(out, ent.stun_recovers_while_held);
    WritePod(out, ent.can_be_picked_up);
    WritePod(out, ent.affected_by_cobweb);
    WritePod(out, ent.can_only_be_picked_up_if_dead_or_stunned);
    WritePod(out, ent.impassable);
    WritePod(out, ent.can_be_hung_on);
    WritePod(out, ent.fall_timer);
    WritePod(out, ent.pos);
    WritePod(out, ent.vel);
    WritePod(out, ent.acc);
    WritePod(out, ent.max_speed);
    WritePod(out, ent.jump_hold_gravity_frames_remaining);
    WritePod(out, ent.throw_velocity_scale);
    WritePod(out, ent.buoyancy);
    WriteEntEffects(out, ent.effects);
    WritePod(out, ent.size);
    WritePod(out, ent.self_light);
    WritePod(out, ent.light_strength);
    WritePod(out, ent.light_color);
    WritePod(out, ent.light_radius);
    WritePod(out, ent.dist_traveled_this_frame);
    WritePod(out, ent.facing);
    WritePod(out, ent.vertical_flip);
    WritePod(out, ent.draw_layer);
    WritePod(out, ent.render_enabled);
    WritePod(out, ent.aframe_animator);
    WritePod(out, ent.jump_delay_frame_count);
    WritePod(out, ent.jumped_this_frame);
    WritePod(out, ent.climb_detach_cooldown);
    WriteOptionalPod(out, ent.hang_side);
    WritePod(out, ent.can_hang_ledge);
    WritePod(out, ent.can_hang_wall);
    WritePod(out, ent.hang_count);
    WritePod(out, ent.holding);
    WriteOptionalPod(out, ent.pickup_effect);
    WritePod(out, ent.money);
    WritePod(out, ent.buyable);
    WriteOptionalPod(out, ent.stage_spawn_index);
    WriteOptionalPod(out, ent.back_vid);
    WritePod(out, ent.attach_mode);
    WritePod(out, ent.use_state);
    WritePod(out, ent.travel_sound_countdown);
    WritePod(out, ent.travel_sound);
    WritePod(out, ent.condition);
    WritePod(out, ent.last_condition);
    WritePod(out, ent.ai_state);
    WritePod(out, ent.last_ai_state);
    WritePod(out, ent.movement_flags);
    WritePod(out, ent.health);
    WritePod(out, ent.hurt_on_contact);
    WritePod(out, ent.vanish_on_death);
    WritePod(out, ent.affected_by_ground_friction);
    WritePod(out, ent.support_ground_friction);
    WritePod(out, ent.push_acc);
    WriteOptionalPod(out, ent.damage_anim);
    WriteOptionalPod(out, ent.damage_sound);
    WriteOptionalPod(out, ent.collide_sound);
    WriteOptionalPod(out, ent.death_sound);
    WritePod(out, ent.on_death);
    WritePod(out, ent.on_damage);
    WritePod(out, ent.on_use);
    WritePod(out, ent.on_area_enter);
    WritePod(out, ent.on_area_exit);
    WritePod(out, ent.on_area_tile_changed);
    WritePod(out, ent.control_logic);
    WritePod(out, ent.step_logic);
    WritePod(out, ent.step_physics);
    WriteOptionalPod(out, ent.transition_target);
    WritePod(out, ent.stage_exit_id);
    WritePod(out, ent.attack_weight);
    WritePod(out, ent.weight);
    WritePod(out, ent.bomb_throw_delay_countdown);
    WritePod(out, ent.rope_throw_delay_countdown);
    WritePod(out, ent.attack_delay_countdown);
    WritePod(out, ent.equip_delay_countdown);
    WriteOptionalPod(out, ent.thrown_by);
    WritePod(out, ent.thrown_immunity_timer);
    WritePod(out, ent.proj_contact_damage_type);
    WritePod(out, ent.proj_contact_damage_amount);
    WritePod(out, ent.can_apply_proj_contact);
    WritePod(out, ent.proj_contact_timer);
    WritePod(out, ent.collided);
    WritePod(out, ent.collided_last_frame);
    WritePod(out, ent.contact_sound_cooldown);
    WritePod(out, ent.damage_vuln);
    WritePod(out, ent.can_be_stunned);
    WritePod(out, ent.point_a);
    WritePod(out, ent.point_b);
    WritePod(out, ent.point_c);
    WritePod(out, ent.point_d);
    WritePod(out, ent.point_label_a);
    WritePod(out, ent.point_label_b);
    WritePod(out, ent.point_label_c);
    WritePod(out, ent.point_label_d);
    WriteOptionalPod(out, ent.holding_vid);
    WriteOptionalPod(out, ent.held_by_vid);
    WritePod(out, ent.holding_timer);
    WriteOptionalPod(out, ent.ent_a);
    WriteOptionalPod(out, ent.ent_b);
    WriteOptionalPod(out, ent.ent_c);
    WriteOptionalPod(out, ent.ent_d);
    WriteOptionalVectorPod(out, ent.child_vids);
    WriteOptionalVectorPod(out, ent.inside_vids);
    WritePod(out, ent.ent_label_a);
    WritePod(out, ent.alignment);
    WritePod(out, ent.counter_a);
    WritePod(out, ent.counter_b);
    WritePod(out, ent.counter_c);
    WritePod(out, ent.counter_d);
    WritePod(out, ent.threshold_a);
    WritePod(out, ent.threshold_b);
}

bool ReadEnt(std::istream& in, Ent& ent) {
    return ReadPod(in, ent.active) &&
           ReadPod(in, ent.marked_for_destruction) &&
           ReadPod(in, ent.type_) &&
           ReadPod(in, ent.vid) &&
           ReadPod(in, ent.has_physics) &&
           ReadPod(in, ent.can_collide) &&
           ReadPod(in, ent.can_be_hit) &&
           ReadPod(in, ent.can_receive_proj_contact) &&
           ReadPod(in, ent.stone) &&
           ReadPod(in, ent.wanted) &&
           ReadPod(in, ent.crusher_pusher) &&
           ReadPod(in, ent.pushable) &&
           ReadPod(in, ent.can_stomp) &&
           ReadPod(in, ent.can_be_stomped) &&
           ReadPod(in, ent.can_collect_pickups) &&
           ReadPod(in, ent.can_go_on_back) &&
           ReadPod(in, ent.grounded) &&
           ReadPod(in, ent.shake) &&
           ReadPod(in, ent.rotation) &&
           ReadPod(in, ent.alpha) &&
           ReadPod(in, ent.coyote_time) &&
           ReadPod(in, ent.stun_timer) &&
           ReadPod(in, ent.stun_recovers_on_ground) &&
           ReadPod(in, ent.stun_recovers_while_held) &&
           ReadPod(in, ent.can_be_picked_up) &&
           ReadPod(in, ent.affected_by_cobweb) &&
           ReadPod(in, ent.can_only_be_picked_up_if_dead_or_stunned) &&
           ReadPod(in, ent.impassable) &&
           ReadPod(in, ent.can_be_hung_on) &&
           ReadPod(in, ent.fall_timer) &&
           ReadPod(in, ent.pos) &&
           ReadPod(in, ent.vel) &&
           ReadPod(in, ent.acc) &&
           ReadPod(in, ent.max_speed) &&
           ReadPod(in, ent.jump_hold_gravity_frames_remaining) &&
           ReadPod(in, ent.throw_velocity_scale) &&
           ReadPod(in, ent.buoyancy) &&
           ReadEntEffects(in, ent.effects) &&
           ReadPod(in, ent.size) &&
           ReadPod(in, ent.self_light) &&
           ReadPod(in, ent.light_strength) &&
           ReadPod(in, ent.light_color) &&
           ReadPod(in, ent.light_radius) &&
           ReadPod(in, ent.dist_traveled_this_frame) &&
           ReadPod(in, ent.facing) &&
           ReadPod(in, ent.vertical_flip) &&
           ReadPod(in, ent.draw_layer) &&
           ReadPod(in, ent.render_enabled) &&
           ReadPod(in, ent.aframe_animator) &&
           ReadPod(in, ent.jump_delay_frame_count) &&
           ReadPod(in, ent.jumped_this_frame) &&
           ReadPod(in, ent.climb_detach_cooldown) &&
           ReadOptionalPod(in, ent.hang_side) &&
           ReadPod(in, ent.can_hang_ledge) &&
           ReadPod(in, ent.can_hang_wall) &&
           ReadPod(in, ent.hang_count) &&
           ReadPod(in, ent.holding) &&
           ReadOptionalPod(in, ent.pickup_effect) &&
           ReadPod(in, ent.money) &&
           ReadPod(in, ent.buyable) &&
           ReadOptionalPod(in, ent.stage_spawn_index) &&
           ReadOptionalPod(in, ent.back_vid) &&
           ReadPod(in, ent.attach_mode) &&
           ReadPod(in, ent.use_state) &&
           ReadPod(in, ent.travel_sound_countdown) &&
           ReadPod(in, ent.travel_sound) &&
           ReadPod(in, ent.condition) &&
           ReadPod(in, ent.last_condition) &&
           ReadPod(in, ent.ai_state) &&
           ReadPod(in, ent.last_ai_state) &&
           ReadPod(in, ent.movement_flags) &&
           ReadPod(in, ent.health) &&
           ReadPod(in, ent.hurt_on_contact) &&
           ReadPod(in, ent.vanish_on_death) &&
           ReadPod(in, ent.affected_by_ground_friction) &&
           ReadPod(in, ent.support_ground_friction) &&
           ReadPod(in, ent.push_acc) &&
           ReadOptionalPod(in, ent.damage_anim) &&
           ReadOptionalPod(in, ent.damage_sound) &&
           ReadOptionalPod(in, ent.collide_sound) &&
           ReadOptionalPod(in, ent.death_sound) &&
           ReadPod(in, ent.on_death) &&
           ReadPod(in, ent.on_damage) &&
           ReadPod(in, ent.on_use) &&
           ReadPod(in, ent.on_area_enter) &&
           ReadPod(in, ent.on_area_exit) &&
           ReadPod(in, ent.on_area_tile_changed) &&
           ReadPod(in, ent.control_logic) &&
           ReadPod(in, ent.step_logic) &&
           ReadPod(in, ent.step_physics) &&
           ReadOptionalPod(in, ent.transition_target) &&
           ReadPod(in, ent.stage_exit_id) &&
           ReadPod(in, ent.attack_weight) &&
           ReadPod(in, ent.weight) &&
           ReadPod(in, ent.bomb_throw_delay_countdown) &&
           ReadPod(in, ent.rope_throw_delay_countdown) &&
           ReadPod(in, ent.attack_delay_countdown) &&
           ReadPod(in, ent.equip_delay_countdown) &&
           ReadOptionalPod(in, ent.thrown_by) &&
           ReadPod(in, ent.thrown_immunity_timer) &&
           ReadPod(in, ent.proj_contact_damage_type) &&
           ReadPod(in, ent.proj_contact_damage_amount) &&
           ReadPod(in, ent.can_apply_proj_contact) &&
           ReadPod(in, ent.proj_contact_timer) &&
           ReadPod(in, ent.collided) &&
           ReadPod(in, ent.collided_last_frame) &&
           ReadPod(in, ent.contact_sound_cooldown) &&
           ReadPod(in, ent.damage_vuln) &&
           ReadPod(in, ent.can_be_stunned) &&
           ReadPod(in, ent.point_a) &&
           ReadPod(in, ent.point_b) &&
           ReadPod(in, ent.point_c) &&
           ReadPod(in, ent.point_d) &&
           ReadPod(in, ent.point_label_a) &&
           ReadPod(in, ent.point_label_b) &&
           ReadPod(in, ent.point_label_c) &&
           ReadPod(in, ent.point_label_d) &&
           ReadOptionalPod(in, ent.holding_vid) &&
           ReadOptionalPod(in, ent.held_by_vid) &&
           ReadPod(in, ent.holding_timer) &&
           ReadOptionalPod(in, ent.ent_a) &&
           ReadOptionalPod(in, ent.ent_b) &&
           ReadOptionalPod(in, ent.ent_c) &&
           ReadOptionalPod(in, ent.ent_d) &&
           ReadOptionalVectorPod(in, ent.child_vids) &&
           ReadOptionalVectorPod(in, ent.inside_vids) &&
           ReadPod(in, ent.ent_label_a) &&
           ReadPod(in, ent.alignment) &&
           ReadPod(in, ent.counter_a) &&
           ReadPod(in, ent.counter_b) &&
           ReadPod(in, ent.counter_c) &&
           ReadPod(in, ent.counter_d) &&
           ReadPod(in, ent.threshold_a) &&
           ReadPod(in, ent.threshold_b);
}

void WriteSettings(std::ostream& out, const Settings& settings) {
    WritePod(out, settings.mode);
    WritePod(out, settings.video.resolution);
    WritePod(out, settings.video.fullscreen);
    WritePod(out, settings.video.vsync);
    WriteVectorPod(out, settings.video.resolution_options);
    WritePod(out, settings.audio.music_volume);
    WritePod(out, settings.audio.sfx_volume);
    WritePod(out, settings.audio.pan_half_width_px);
    WritePod(out, settings.controls.jump);
    WritePod(out, settings.controls.shoot);
    WritePod(out, settings.ui.icon_scale);
    WritePod(out, settings.ui.status_icon_scale);
    WritePod(out, settings.ui.tool_slot_scale);
    WritePod(out, settings.ui.tool_icon_scale);
    WritePod(out, settings.post_process.effect);
    WritePod(out, settings.post_process.terrain_lighting);
    WritePod(out, settings.post_process.terrain_seam_ao);
    WritePod(out, settings.post_process.terrain_exposure_lighting);
    WritePod(out, settings.post_process.backwall_lighting);
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
    WritePod(out, settings.player_tuning);
}

bool ReadSettings(std::istream& in, Settings& settings) {
    if (!ReadPod(in, settings.mode) ||
        !ReadPod(in, settings.video.resolution) ||
        !ReadPod(in, settings.video.fullscreen) ||
        !ReadPod(in, settings.video.vsync) ||
        !ReadVectorPod(in, settings.video.resolution_options) ||
        !ReadPod(in, settings.audio.music_volume) ||
        !ReadPod(in, settings.audio.sfx_volume) ||
        !ReadPod(in, settings.audio.pan_half_width_px) ||
        !ReadPod(in, settings.controls.jump) ||
        !ReadPod(in, settings.controls.shoot) ||
        !ReadPod(in, settings.ui.icon_scale) ||
        !ReadPod(in, settings.ui.status_icon_scale) ||
        !ReadPod(in, settings.ui.tool_slot_scale) ||
        !ReadPod(in, settings.ui.tool_icon_scale) ||
        !ReadPod(in, settings.post_process.effect) ||
        !ReadPod(in, settings.post_process.terrain_lighting) ||
        !ReadPod(in, settings.post_process.terrain_seam_ao) ||
        !ReadPod(in, settings.post_process.terrain_exposure_lighting) ||
        !ReadPod(in, settings.post_process.backwall_lighting) ||
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
        !ReadPod(in, settings.post_process.crt_brightness_boost) ||
        !ReadPod(in, settings.player_tuning)) {
        return false;
    }
    return true;
}

void WriteStageExitRequirement(std::ostream& out, const StageExitRequirement& requirement) {
    WriteString(out, requirement.flag);
    WritePod(out, requirement.expected);
}

bool ReadStageExitRequirement(std::istream& in, StageExitRequirement& requirement) {
    return ReadString(in, requirement.flag) &&
           ReadPod(in, requirement.expected);
}

void WriteStageExitTarget(std::ostream& out, const StageExitTarget& target) {
    WriteString(out, target.target_stage_id);
    const std::uint32_t count = static_cast<std::uint32_t>(target.requirements.size());
    WritePod(out, count);
    for (const StageExitRequirement& requirement : target.requirements) {
        WriteStageExitRequirement(out, requirement);
    }
}

bool ReadStageExitTarget(std::istream& in, StageExitTarget& target) {
    if (!ReadString(in, target.target_stage_id)) {
        return false;
    }
    std::uint32_t count = 0;
    if (!ReadPod(in, count)) {
        return false;
    }
    target.requirements.resize(count);
    for (StageExitRequirement& requirement : target.requirements) {
        if (!ReadStageExitRequirement(in, requirement)) {
            return false;
        }
    }
    return true;
}

void WriteStageExit(std::ostream& out, const StageExit& exit) {
    WriteString(out, exit.id);
    WriteStageExitTarget(out, exit.target);
}

bool ReadStageExit(std::istream& in, StageExit& exit) {
    return ReadString(in, exit.id) &&
           ReadStageExitTarget(in, exit.target);
}

void WriteEntSpawn(std::ostream& out, const EntSpawn& spawn) {
    WritePod(out, spawn.type_);
    WritePod(out, spawn.pos);
    WriteOptionalPod(out, spawn.size_override);
    WritePod(out, spawn.facing);
    WriteOptionalPod(out, spawn.ai_state_override);
    WritePod(out, spawn.anim_id);
    WriteOptionalPod(out, spawn.ent_a_spawn_index);
    WriteOptionalPod(out, spawn.ent_b_spawn_index);
    WriteOptionalPod(out, spawn.ent_c_spawn_index);
    WriteOptionalPod(out, spawn.ent_d_spawn_index);
    WriteOptionalPod(out, spawn.shop_owner_spawn_index);
    WritePod(out, spawn.buyable);
    WritePod(out, spawn.buy_price);
    WriteString(out, spawn.exit_id);
}

bool ReadEntSpawn(std::istream& in, EntSpawn& spawn) {
    return ReadPod(in, spawn.type_) &&
           ReadPod(in, spawn.pos) &&
           ReadOptionalPod(in, spawn.size_override) &&
           ReadPod(in, spawn.facing) &&
           ReadOptionalPod(in, spawn.ai_state_override) &&
           ReadPod(in, spawn.anim_id) &&
           ReadOptionalPod(in, spawn.ent_a_spawn_index) &&
           ReadOptionalPod(in, spawn.ent_b_spawn_index) &&
           ReadOptionalPod(in, spawn.ent_c_spawn_index) &&
           ReadOptionalPod(in, spawn.ent_d_spawn_index) &&
           ReadOptionalPod(in, spawn.shop_owner_spawn_index) &&
           ReadPod(in, spawn.buyable) &&
           ReadPod(in, spawn.buy_price) &&
           ReadString(in, spawn.exit_id);
}

void WriteStageGenAnnotation(std::ostream& out, const StageGenAnnotation& annotation) {
    WritePod(out, annotation.world_pos);
    WriteString(out, annotation.text);
}

bool ReadStageGenAnnotation(std::istream& in, StageGenAnnotation& annotation) {
    return ReadPod(in, annotation.world_pos) &&
           ReadString(in, annotation.text);
}

template <typename T>
void WriteGridPod(std::ostream& out, const std::vector<std::vector<T>>& grid) {
    const std::uint32_t rows = static_cast<std::uint32_t>(grid.size());
    WritePod(out, rows);
    for (const std::vector<T>& row : grid) {
        WriteVectorPod(out, row);
    }
}

template <typename T>
bool ReadGridPod(std::istream& in, std::vector<std::vector<T>>& grid) {
    std::uint32_t rows = 0;
    if (!ReadPod(in, rows)) {
        return false;
    }
    grid.resize(rows);
    for (std::uint32_t i = 0; i < rows; ++i) {
        if (!ReadVectorPod(in, grid[i])) {
            return false;
        }
    }
    return true;
}

void WriteStage(std::ostream& out, const Stage& stage) {
    WritePod(out, stage.stage_type);
    WriteString(out, stage.quest_id);
    WriteString(out, stage.quest_stage_id);
    WriteString(out, stage.route_label);
    WriteString(out, stage.stage_title);
    WritePod(out, stage.quest_level_number);
    WriteOptionalPod(out, stage.generation_seed);
    const std::uint32_t exit_count = static_cast<std::uint32_t>(stage.exits.size());
    WritePod(out, exit_count);
    for (const StageExit& exit : stage.exits) {
        WriteStageExit(out, exit);
    }
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
    WritePod(out, stage.wrap_padding_tiles);
    WritePod(out, stage.wrap_core_origin_tiles);
    WritePod(out, stage.wrap_core_size_tiles);
    WriteGridPod(out, stage.tiles);
    WriteGridPod(out, stage.tile_rotations);
    WriteGridPod(out, stage.fluid_tiles);
    WriteGridPod(out, stage.fluid_amount);
    WriteGridPod(out, stage.fluid_display_amount);
    WriteGridPod(out, stage.fluid_velocity);
    WriteGridPod(out, stage.fluid_gravity);
    WriteGridPod(out, stage.fluid_gravity_strength);
    WriteGridPod(out, stage.fluid_temp_gravity);
    WriteGridPod(out, stage.tile_shake);
    WriteGridPod(out, stage.backwall_tile_shake);
    WriteGridPod(out, stage.backwall_tiles);
    WriteVectorPod(out, stage.backwall_fill_tiles);
    WriteGridPod(out, stage.embedded_treasures);
    WriteGridPod(out, stage.rooms);
    WriteVectorPod(out, stage.path);
    const std::uint32_t spawn_count = static_cast<std::uint32_t>(stage.ent_spawns.size());
    WritePod(out, spawn_count);
    for (const EntSpawn& spawn : stage.ent_spawns) {
        WriteEntSpawn(out, spawn);
    }
    WriteVectorPod(out, stage.background_stamps);
    const std::uint32_t annotation_count =
        static_cast<std::uint32_t>(stage.stagegen_annotations.size());
    WritePod(out, annotation_count);
    for (const StageGenAnnotation& annotation : stage.stagegen_annotations) {
        WriteStageGenAnnotation(out, annotation);
    }
    WriteVectorPod(out, stage.lights);
    WritePod(out, stage.block_anim_id);
    WritePod(out, stage.next_light_vid);
    WritePod(out, stage.tile_change_generation);
}

bool ReadStage(std::istream& in, Stage& stage) {
    if (!ReadPod(in, stage.stage_type) ||
        !ReadString(in, stage.quest_id) ||
        !ReadString(in, stage.quest_stage_id) ||
        !ReadString(in, stage.route_label) ||
        !ReadString(in, stage.stage_title) ||
        !ReadPod(in, stage.quest_level_number) ||
        !ReadOptionalPod(in, stage.generation_seed)) {
        return false;
    }

    std::uint32_t exit_count = 0;
    if (!ReadPod(in, exit_count)) {
        return false;
    }
    stage.exits.resize(exit_count);
    for (StageExit& exit : stage.exits) {
        if (!ReadStageExit(in, exit)) {
            return false;
        }
    }

    if (!ReadPod(in, stage.gravity) ||
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
        !ReadPod(in, stage.wrap_padding_tiles) ||
        !ReadPod(in, stage.wrap_core_origin_tiles) ||
        !ReadPod(in, stage.wrap_core_size_tiles)) {
        return false;
    }

    if (!ReadGridPod(in, stage.tiles) ||
        !ReadGridPod(in, stage.tile_rotations) ||
        !ReadGridPod(in, stage.fluid_tiles) ||
        !ReadGridPod(in, stage.fluid_amount) ||
        !ReadGridPod(in, stage.fluid_display_amount) ||
        !ReadGridPod(in, stage.fluid_velocity) ||
        !ReadGridPod(in, stage.fluid_gravity) ||
        !ReadGridPod(in, stage.fluid_gravity_strength) ||
        !ReadGridPod(in, stage.fluid_temp_gravity) ||
        !ReadGridPod(in, stage.tile_shake) ||
        !ReadGridPod(in, stage.backwall_tile_shake) ||
        !ReadGridPod(in, stage.backwall_tiles) ||
        !ReadVectorPod(in, stage.backwall_fill_tiles) ||
        !ReadGridPod(in, stage.embedded_treasures) ||
        !ReadGridPod(in, stage.rooms) ||
        !ReadVectorPod(in, stage.path)) {
        return false;
    }

    std::uint32_t spawn_count = 0;
    if (!ReadPod(in, spawn_count)) {
        return false;
    }
    stage.ent_spawns.resize(spawn_count);
    for (EntSpawn& spawn : stage.ent_spawns) {
        if (!ReadEntSpawn(in, spawn)) {
            return false;
        }
    }

    if (!ReadVectorPod(in, stage.background_stamps)) {
        return false;
    }
    std::uint32_t annotation_count = 0;
    if (!ReadPod(in, annotation_count)) {
        return false;
    }
    stage.stagegen_annotations.resize(annotation_count);
    for (StageGenAnnotation& annotation : stage.stagegen_annotations) {
        if (!ReadStageGenAnnotation(in, annotation)) {
            return false;
        }
    }

    return ReadVectorPod(in, stage.lights) &&
           ReadPod(in, stage.block_anim_id) &&
           ReadPod(in, stage.next_light_vid) &&
           ReadPod(in, stage.tile_change_generation);
}

void WriteEntPool(std::ostream& out, const EntPool& ents) {
    const std::uint32_t ent_count = static_cast<std::uint32_t>(ents.ents.size());
    WritePod(out, ent_count);
    for (const Ent& ent : ents.ents) {
        WriteEnt(out, ent);
    }
    WriteVectorPod(out, ents.available_ids);
}

bool ReadEntPool(std::istream& in, EntPool& ents) {
    std::uint32_t ent_count = 0;
    if (!ReadPod(in, ent_count)) {
        return false;
    }

    ents.ents.resize(ent_count);
    for (std::uint32_t i = 0; i < ent_count; ++i) {
        if (!ReadEnt(in, ents.ents[i])) {
            return false;
        }
    }

    return ReadVectorPod(in, ents.available_ids);
}

void WritePlayerRegistry(std::ostream& out, const PlayerRegistry& players) {
    const std::uint32_t count = static_cast<std::uint32_t>(players.slots.size());
    WritePod(out, count);
    for (const PlayerSlot& slot : players.slots) {
        WritePod(out, slot.player_id);
        WriteOptionalPod(out, slot.ent_vid);
        WritePod(out, slot.connection_kind);
        WritePod(out, slot.connected);
        WritePod(out, slot.primary_local);
        WriteString(out, slot.display_name);
        WritePod(out, slot.input_frame);
        WritePod(out, slot.previous_input_frame);
        WritePod(out, slot.inputs);
        WritePod(out, slot.immediate_inputs);
    }
}

bool ReadPlayerRegistry(std::istream& in, PlayerRegistry& players) {
    std::uint32_t count = 0;
    if (!ReadPod(in, count)) {
        return false;
    }
    players.slots.resize(count);
    for (PlayerSlot& slot : players.slots) {
        if (!ReadPod(in, slot.player_id) ||
            !ReadOptionalPod(in, slot.ent_vid) ||
            !ReadPod(in, slot.connection_kind) ||
            !ReadPod(in, slot.connected) ||
            !ReadPod(in, slot.primary_local) ||
            !ReadString(in, slot.display_name) ||
            !ReadPod(in, slot.input_frame) ||
            !ReadPod(in, slot.previous_input_frame) ||
            !ReadPod(in, slot.inputs) ||
            !ReadPod(in, slot.immediate_inputs)) {
            return false;
        }
    }
    return true;
}

void WriteContactBookkeeping(std::ostream& out, const ContactBookkeeping& contact) {
    WriteVectorPod(out, contact.contact_cooldowns);
    WriteVectorPod(out, contact.interaction_cooldowns);
    WriteVectorPod(out, contact.ent_contact_dispatches_this_tick);
    WriteVectorPod(out, contact.proj_body_impact_cooldowns);
}

bool ReadContactBookkeeping(std::istream& in, ContactBookkeeping& contact) {
    return ReadVectorPod(in, contact.contact_cooldowns) &&
           ReadVectorPod(in, contact.interaction_cooldowns) &&
           ReadVectorPod(in, contact.ent_contact_dispatches_this_tick) &&
           ReadVectorPod(in, contact.proj_body_impact_cooldowns);
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
    WritePod(out, snapshot.debug_shake_brush);
    WritePod(out, snapshot.debug_audio_brush);
    WritePod(out, snapshot.debug_fluid_brush);
    WritePod(out, snapshot.stage_rotation);
    WritePod(out, snapshot.player_tuning);
    WritePod(out, snapshot.now);
    WritePod(out, snapshot.time_since_last_update);
    WritePod(out, snapshot.scene_frame);
    WritePod(out, snapshot.frame);
    WritePod(out, snapshot.stage_frame);
    WritePod(out, snapshot.drng);
    WritePod(out, snapshot.stagegen_drng);
    WritePod(out, snapshot.menu_return_to);
    WritePod(out, snapshot.game_over);
    WritePod(out, snapshot.pause);
    WritePod(out, snapshot.win);
    WritePod(out, snapshot.respawn_target);
    WriteOptionalPod(out, snapshot.pending_stage_transition);
    WritePod(out, snapshot.multiplayer_respawn_mode);
    WritePod(out, snapshot.points);
    WritePod(out, snapshot.deaths);
    WritePod(out, snapshot.depth);
    WritePod(out, snapshot.sac_altar_favor);
    WritePod(out, snapshot.sac_altar_reward_tier);
    WritePod(out, snapshot.quest_state);
    WritePlayerRegistry(out, snapshot.players);
    WritePod(out, snapshot.frame_pause);
    WritePod(out, snapshot.debug_level);
    WriteEntPool(out, snapshot.ents);
    WriteStage(out, snapshot.stage);
    WriteOptionalPod(out, snapshot.controlled_ent_vid);
    WriteOptionalPod(out, snapshot.spectator_target_player_id);
    WriteOptionalPod(out, snapshot.mouse_trailer_vid);
    WriteContactBookkeeping(out, snapshot.contact);
    WriteVectorPod(out, snapshot.ent_tool_states);
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
           ReadPod(in, snapshot.debug_shake_brush) &&
           ReadPod(in, snapshot.debug_audio_brush) &&
           ReadPod(in, snapshot.debug_fluid_brush) &&
           ReadPod(in, snapshot.stage_rotation) &&
           ReadPod(in, snapshot.player_tuning) &&
           ReadPod(in, snapshot.now) &&
           ReadPod(in, snapshot.time_since_last_update) &&
           ReadPod(in, snapshot.scene_frame) &&
           ReadPod(in, snapshot.frame) &&
           ReadPod(in, snapshot.stage_frame) &&
           ReadPod(in, snapshot.drng) &&
           ReadPod(in, snapshot.stagegen_drng) &&
           ReadPod(in, snapshot.menu_return_to) &&
           ReadPod(in, snapshot.game_over) &&
           ReadPod(in, snapshot.pause) &&
           ReadPod(in, snapshot.win) &&
           ReadPod(in, snapshot.respawn_target) &&
           ReadOptionalPod(in, snapshot.pending_stage_transition) &&
           ReadPod(in, snapshot.multiplayer_respawn_mode) &&
           ReadPod(in, snapshot.points) &&
           ReadPod(in, snapshot.deaths) &&
           ReadPod(in, snapshot.depth) &&
           ReadPod(in, snapshot.sac_altar_favor) &&
           ReadPod(in, snapshot.sac_altar_reward_tier) &&
           ReadPod(in, snapshot.quest_state) &&
           ReadPlayerRegistry(in, snapshot.players) &&
           ReadPod(in, snapshot.frame_pause) &&
           ReadPod(in, snapshot.debug_level) &&
           ReadEntPool(in, snapshot.ents) &&
           ReadStage(in, snapshot.stage) &&
           ReadOptionalPod(in, snapshot.controlled_ent_vid) &&
           ReadOptionalPod(in, snapshot.spectator_target_player_id) &&
           ReadOptionalPod(in, snapshot.mouse_trailer_vid) &&
           ReadContactBookkeeping(in, snapshot.contact) &&
           ReadVectorPod(in, snapshot.ent_tool_states) &&
           ReadPod(in, snapshot.play_cam_pos);
}

void WriteSimPlayerSlotSnapshot(std::ostream& out, const SimPlayerSlotSnapshot& slot) {
    WritePod(out, slot.player_id);
    WriteOptionalPod(out, slot.ent_vid);
    WritePod(out, slot.connected);
    WriteString(out, slot.display_name);
    WritePod(out, slot.input_frame);
    WritePod(out, slot.previous_input_frame);
    WritePod(out, slot.inputs);
    WritePod(out, slot.immediate_inputs);
}

bool ReadSimPlayerSlotSnapshot(std::istream& in, SimPlayerSlotSnapshot& slot) {
    return ReadPod(in, slot.player_id) &&
           ReadOptionalPod(in, slot.ent_vid) &&
           ReadPod(in, slot.connected) &&
           ReadString(in, slot.display_name) &&
           ReadPod(in, slot.input_frame) &&
           ReadPod(in, slot.previous_input_frame) &&
           ReadPod(in, slot.inputs) &&
           ReadPod(in, slot.immediate_inputs);
}

void WriteSimPlayerSlots(std::ostream& out, const std::vector<SimPlayerSlotSnapshot>& slots) {
    const std::uint32_t count = static_cast<std::uint32_t>(slots.size());
    WritePod(out, count);
    for (const SimPlayerSlotSnapshot& slot : slots) {
        WriteSimPlayerSlotSnapshot(out, slot);
    }
}

bool ReadSimPlayerSlots(std::istream& in, std::vector<SimPlayerSlotSnapshot>& slots) {
    std::uint32_t count = 0;
    if (!ReadPod(in, count)) {
        return false;
    }
    slots.resize(count);
    for (SimPlayerSlotSnapshot& slot : slots) {
        if (!ReadSimPlayerSlotSnapshot(in, slot)) {
            return false;
        }
    }
    return true;
}

void WriteSimNetEntLinks(
    std::ostream& out,
    const std::vector<SimNetEntLinkSnapshot>& links
) {
    WriteVectorPod(out, links);
}

bool ReadSimNetEntLinks(
    std::istream& in,
    std::vector<SimNetEntLinkSnapshot>& links
) {
    return ReadVectorPod(in, links);
}

void WriteSimNetEntIdAliases(
    std::ostream& out,
    const std::vector<SimNetEntIdAliasSnapshot>& aliases
) {
    WriteVectorPod(out, aliases);
}

bool ReadSimNetEntIdAliases(
    std::istream& in,
    std::vector<SimNetEntIdAliasSnapshot>& aliases
) {
    return ReadVectorPod(in, aliases);
}

void WriteSimSnapshot(std::ostream& out, const SimSnapshot& snapshot) {
    WritePod(out, snapshot.mode);
    WriteSettings(out, snapshot.settings);
    WritePod(out, snapshot.playing_inputs);
    WritePod(out, snapshot.immediate_playing_inputs);
    WritePod(out, snapshot.playing_input_snapshot);
    WritePod(out, snapshot.previous_playing_input_snapshot);
    WritePod(out, snapshot.previous_immediate_playing_input_snapshot);
    WritePod(out, snapshot.stage_rotation);
    WritePod(out, snapshot.player_tuning);
    WritePod(out, snapshot.running);
    WritePod(out, snapshot.now);
    WritePod(out, snapshot.time_since_last_update);
    WritePod(out, snapshot.scene_frame);
    WritePod(out, snapshot.frame);
    WritePod(out, snapshot.stage_frame);
    WritePod(out, snapshot.drng);
    WritePod(out, snapshot.stagegen_drng);
    WritePod(out, snapshot.menu_return_to);
    WritePod(out, snapshot.game_over);
    WritePod(out, snapshot.pause);
    WritePod(out, snapshot.win);
    WritePod(out, snapshot.respawn_target);
    WriteOptionalPod(out, snapshot.pending_stage_transition);
    WritePod(out, snapshot.multiplayer_respawn_mode);
    WritePod(out, snapshot.points);
    WritePod(out, snapshot.deaths);
    WritePod(out, snapshot.depth);
    WritePod(out, snapshot.sac_altar_favor);
    WritePod(out, snapshot.sac_altar_reward_tier);
    WriteVectorPod(out, snapshot.interact_claimed_vids_this_frame);
    WritePod(out, snapshot.quest_state);
    WriteSimPlayerSlots(out, snapshot.players);
    WritePod(out, snapshot.frame_pause);
    WritePod(out, snapshot.debug_level);
    WriteEntPool(out, snapshot.ents);
    WriteVectorPod(out, snapshot.area_listener_vids);
    WriteStage(out, snapshot.stage);
    WriteContactBookkeeping(out, snapshot.contact);
    WriteVectorPod(out, snapshot.ent_tool_states);
    WritePod(out, snapshot.net_next_local_ent_id);
    WriteSimNetEntLinks(out, snapshot.net_ent_links);
    WriteSimNetEntIdAliases(out, snapshot.net_ent_id_aliases);
}

bool ReadSimSnapshot(std::istream& in, SimSnapshot& snapshot) {
    return ReadPod(in, snapshot.mode) &&
           ReadSettings(in, snapshot.settings) &&
           ReadPod(in, snapshot.playing_inputs) &&
           ReadPod(in, snapshot.immediate_playing_inputs) &&
           ReadPod(in, snapshot.playing_input_snapshot) &&
           ReadPod(in, snapshot.previous_playing_input_snapshot) &&
           ReadPod(in, snapshot.previous_immediate_playing_input_snapshot) &&
           ReadPod(in, snapshot.stage_rotation) &&
           ReadPod(in, snapshot.player_tuning) &&
           ReadPod(in, snapshot.running) &&
           ReadPod(in, snapshot.now) &&
           ReadPod(in, snapshot.time_since_last_update) &&
           ReadPod(in, snapshot.scene_frame) &&
           ReadPod(in, snapshot.frame) &&
           ReadPod(in, snapshot.stage_frame) &&
           ReadPod(in, snapshot.drng) &&
           ReadPod(in, snapshot.stagegen_drng) &&
           ReadPod(in, snapshot.menu_return_to) &&
           ReadPod(in, snapshot.game_over) &&
           ReadPod(in, snapshot.pause) &&
           ReadPod(in, snapshot.win) &&
           ReadPod(in, snapshot.respawn_target) &&
           ReadOptionalPod(in, snapshot.pending_stage_transition) &&
           ReadPod(in, snapshot.multiplayer_respawn_mode) &&
           ReadPod(in, snapshot.points) &&
           ReadPod(in, snapshot.deaths) &&
           ReadPod(in, snapshot.depth) &&
           ReadPod(in, snapshot.sac_altar_favor) &&
           ReadPod(in, snapshot.sac_altar_reward_tier) &&
           ReadVectorPod(in, snapshot.interact_claimed_vids_this_frame) &&
           ReadPod(in, snapshot.quest_state) &&
           ReadSimPlayerSlots(in, snapshot.players) &&
           ReadPod(in, snapshot.frame_pause) &&
           ReadPod(in, snapshot.debug_level) &&
           ReadEntPool(in, snapshot.ents) &&
           ReadVectorPod(in, snapshot.area_listener_vids) &&
           ReadStage(in, snapshot.stage) &&
           ReadContactBookkeeping(in, snapshot.contact) &&
           ReadVectorPod(in, snapshot.ent_tool_states) &&
           ReadPod(in, snapshot.net_next_local_ent_id) &&
           ReadSimNetEntLinks(in, snapshot.net_ent_links) &&
           ReadSimNetEntIdAliases(in, snapshot.net_ent_id_aliases);
}

} // namespace

std::vector<std::uint8_t> SerializeGameplaySnapshotToBytes(const GameplaySnapshot& snapshot) {
    std::ostringstream out(std::ios::out | std::ios::binary);
    WriteSnapshot(out, snapshot);
    const std::string text = out.str();
    return std::vector<std::uint8_t>(text.begin(), text.end());
}

bool DeserializeGameplaySnapshotFromBytes(
    const std::vector<std::uint8_t>& bytes,
    GameplaySnapshot& snapshot
) {
    const std::string text(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    std::istringstream in(text, std::ios::in | std::ios::binary);
    return ReadSnapshot(in, snapshot);
}

std::vector<std::uint8_t> SerializeSimSnapshotToBytes(const SimSnapshot& snapshot) {
    std::ostringstream out(std::ios::out | std::ios::binary);
    WriteSimSnapshot(out, snapshot);
    const std::string text = out.str();
    return std::vector<std::uint8_t>(text.begin(), text.end());
}

bool DeserializeSimSnapshotFromBytes(
    const std::vector<std::uint8_t>& bytes,
    SimSnapshot& snapshot
) {
    const std::string text(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    std::istringstream in(text, std::ios::in | std::ios::binary);
    return ReadSimSnapshot(in, snapshot);
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

} // namespace splonks::debug_playback_internal

namespace splonks {

std::vector<std::uint8_t> SerializeGameplaySnapshotToBytes(const GameplaySnapshot& snapshot) {
    return debug_playback_internal::SerializeGameplaySnapshotToBytes(snapshot);
}

bool DeserializeGameplaySnapshotFromBytes(
    const std::vector<std::uint8_t>& bytes,
    GameplaySnapshot& snapshot
) {
    return debug_playback_internal::DeserializeGameplaySnapshotFromBytes(bytes, snapshot);
}

std::vector<std::uint8_t> SerializeSimSnapshotToBytes(const SimSnapshot& snapshot) {
    return debug_playback_internal::SerializeSimSnapshotToBytes(snapshot);
}

bool DeserializeSimSnapshotFromBytes(
    const std::vector<std::uint8_t>& bytes,
    SimSnapshot& snapshot
) {
    return debug_playback_internal::DeserializeSimSnapshotFromBytes(bytes, snapshot);
}

} // namespace splonks

#include "debug/playback_internal.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <type_traits>

namespace splonks::debug_playback_internal {

namespace {

constexpr std::uint32_t kRecordingMagic = 0x53504C52U;
constexpr std::uint32_t kRecordingVersion = 83;

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
    const std::uint8_t has_value = value.has_value() ? 1U : 0U;
    WritePod(out, has_value);
    if (has_value) {
        WritePod(out, *value);
    }
}

template <typename T>
bool ReadOptionalPod(std::istream& in, std::optional<T>& value) {
    static_assert(std::is_trivially_copyable_v<T>);
    std::uint8_t has_value = 0;
    if (!ReadPod(in, has_value)) {
        return false;
    }
    if (has_value == 0) {
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

void WriteOptionalSizeIndex(std::ostream& out, const std::optional<std::size_t>& value) {
    const std::uint8_t has_value = value.has_value() ? 1U : 0U;
    WritePod(out, has_value);
    if (has_value) {
        const std::uint32_t stored = static_cast<std::uint32_t>(*value);
        WritePod(out, stored);
    }
}

bool ReadOptionalSizeIndex(std::istream& in, std::optional<std::size_t>& value) {
    std::uint8_t has_value = 0;
    if (!ReadPod(in, has_value)) {
        return false;
    }
    if (has_value == 0) {
        value.reset();
        return true;
    }
    std::uint32_t loaded = 0;
    if (!ReadPod(in, loaded)) {
        return false;
    }
    value = static_cast<std::size_t>(loaded);
    return true;
}

void WriteSizeIndex(std::ostream& out, const std::size_t value) {
    const std::uint32_t stored = static_cast<std::uint32_t>(value);
    WritePod(out, stored);
}

bool ReadSizeIndex(std::istream& in, std::size_t& value) {
    std::uint32_t loaded = 0;
    if (!ReadPod(in, loaded)) {
        return false;
    }
    value = static_cast<std::size_t>(loaded);
    return true;
}

void WriteSizeIndexVector(std::ostream& out, const std::vector<std::size_t>& values) {
    const std::uint32_t count = static_cast<std::uint32_t>(values.size());
    WritePod(out, count);
    for (const std::size_t value : values) {
        const std::uint32_t stored = static_cast<std::uint32_t>(value);
        WritePod(out, stored);
    }
}

bool ReadSizeIndexVector(std::istream& in, std::vector<std::size_t>& values) {
    std::uint32_t count = 0;
    if (!ReadPod(in, count)) {
        return false;
    }
    values.resize(count);
    for (std::size_t i = 0; i < values.size(); ++i) {
        std::uint32_t loaded = 0;
        if (!ReadPod(in, loaded)) {
            return false;
        }
        values[i] = static_cast<std::size_t>(loaded);
    }
    return true;
}

void WriteVid(std::ostream& out, const VID& vid) {
    WritePod(out, vid.id);
    WritePod(out, vid.version);
}

bool ReadVid(std::istream& in, VID& vid) {
    return ReadPod(in, vid.id) &&
           ReadPod(in, vid.version);
}

void WriteOptionalVid(std::ostream& out, const std::optional<VID>& value) {
    const std::uint8_t has_value = value.has_value() ? 1U : 0U;
    WritePod(out, has_value);
    if (value.has_value()) {
        WriteVid(out, *value);
    }
}

bool ReadOptionalVid(std::istream& in, std::optional<VID>& value) {
    std::uint8_t has_value = 0;
    if (!ReadPod(in, has_value)) {
        return false;
    }
    if (has_value == 0) {
        value.reset();
        return true;
    }
    VID loaded{};
    if (!ReadVid(in, loaded)) {
        return false;
    }
    value = loaded;
    return true;
}

void WriteVidVector(std::ostream& out, const std::vector<VID>& values) {
    const std::uint32_t count = static_cast<std::uint32_t>(values.size());
    WritePod(out, count);
    for (const VID& value : values) {
        WriteVid(out, value);
    }
}

bool ReadVidVector(std::istream& in, std::vector<VID>& values) {
    std::uint32_t count = 0;
    if (!ReadPod(in, count)) {
        return false;
    }
    values.resize(count);
    for (VID& value : values) {
        if (!ReadVid(in, value)) {
            return false;
        }
    }
    return true;
}

void WriteOptionalVidVector(std::ostream& out, const std::optional<std::vector<VID>>& values) {
    const std::uint8_t has_value = values.has_value() ? 1U : 0U;
    WritePod(out, has_value);
    if (values.has_value()) {
        WriteVidVector(out, *values);
    }
}

bool ReadOptionalVidVector(std::istream& in, std::optional<std::vector<VID>>& values) {
    std::uint8_t has_value = 0;
    if (!ReadPod(in, has_value)) {
        return false;
    }
    if (has_value == 0) {
        values.reset();
        return true;
    }
    values.emplace();
    return ReadVidVector(in, *values);
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

void WriteBoolByte(std::ostream& out, bool value) {
    const std::uint8_t stored = value ? 1U : 0U;
    WritePod(out, stored);
}

bool ReadBoolByte(std::istream& in, bool& value) {
    std::uint8_t stored = 0;
    if (!ReadPod(in, stored)) {
        return false;
    }
    value = stored != 0;
    return true;
}

void WriteOptionalBoolByte(std::ostream& out, const std::optional<bool>& value) {
    const std::uint8_t has_value = value.has_value() ? 1U : 0U;
    WritePod(out, has_value);
    if (has_value) {
        WriteBoolByte(out, *value);
    }
}

bool ReadOptionalBoolByte(std::istream& in, std::optional<bool>& value) {
    std::uint8_t has_value = 0;
    if (!ReadPod(in, has_value)) {
        return false;
    }
    if (has_value == 0) {
        value.reset();
        return true;
    }
    bool loaded = false;
    if (!ReadBoolByte(in, loaded)) {
        return false;
    }
    value = loaded;
    return true;
}

void WritePlayerConnectionKind(std::ostream& out, PlayerConnectionKind kind) {
    const std::uint8_t stored = static_cast<std::uint8_t>(kind);
    WritePod(out, stored);
}

bool ReadPlayerConnectionKind(std::istream& in, PlayerConnectionKind& kind) {
    std::uint8_t stored = 0;
    if (!ReadPod(in, stored)) {
        return false;
    }
    if (stored > static_cast<std::uint8_t>(PlayerConnectionKind::Remote)) {
        return false;
    }
    kind = static_cast<PlayerConnectionKind>(stored);
    return true;
}

void WriteEffectInstance(std::ostream& out, const EffectInstance& effect) {
    const std::uint8_t id = static_cast<std::uint8_t>(effect.id);
    WritePod(out, id);
    WritePod(out, effect.count);
    WritePod(out, effect.value);
    WritePod(out, effect.frames_remaining);
}

bool ReadEffectInstance(std::istream& in, EffectInstance& effect) {
    std::uint8_t id = 0;
    std::int32_t count = 0;
    float value = 0.0F;
    std::uint32_t frames_remaining = 0;
    if (!ReadPod(in, id) ||
        !ReadPod(in, count) ||
        !ReadPod(in, value) ||
        !ReadPod(in, frames_remaining)) {
        return false;
    }
    if (id >= static_cast<std::uint8_t>(EffectId::Count)) {
        return false;
    }
    effect.id = static_cast<EffectId>(id);
    effect.count = count;
    effect.value = value;
    effect.frames_remaining = frames_remaining;
    return true;
}

void WriteEntEffects(std::ostream& out, const BoxedEntEffects& effects_box) {
    const EntEffects* const effects = effects_box.get();
    const std::uint8_t count = effects != nullptr ? effects->count : 0;
    WritePod(out, count);
    for (std::uint8_t i = 0; i < count; ++i) {
        WriteEffectInstance(out, effects->effects[i]);
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
        if (!ReadEffectInstance(in, effects.effects[i])) {
            return false;
        }
    }
    return true;
}

void WriteAFrameAnimator(std::ostream& out, const AFrameAnimator& animator) {
    WritePod(out, animator.anim_id);
    WriteSizeIndex(out, animator.current_frame);
    WritePod(out, animator.current_time);
    WritePod(out, animator.scale);
    WritePod(out, animator.speed);
    WriteBoolByte(out, animator.animate);
    WriteBoolByte(out, animator.loop);
    WriteBoolByte(out, animator.finished);
    const std::uint8_t playback_mode = static_cast<std::uint8_t>(animator.playback_mode);
    WritePod(out, playback_mode);
    WritePod(out, animator.play_count);
    WritePod(out, animator.plays_completed);
    WriteBoolByte(out, animator.playback_dirty);
    WriteBoolByte(out, animator.ping_pong_forward);
}

bool ReadAFrameAnimator(std::istream& in, AFrameAnimator& animator) {
    std::uint8_t playback_mode = 0;
    bool animate = false;
    bool loop = false;
    bool finished = false;
    bool playback_dirty = false;
    bool ping_pong_forward = false;
    if (!ReadPod(in, animator.anim_id) ||
        !ReadSizeIndex(in, animator.current_frame) ||
        !ReadPod(in, animator.current_time) ||
        !ReadPod(in, animator.scale) ||
        !ReadPod(in, animator.speed) ||
        !ReadBoolByte(in, animate) ||
        !ReadBoolByte(in, loop) ||
        !ReadBoolByte(in, finished) ||
        !ReadPod(in, playback_mode) ||
        !ReadPod(in, animator.play_count) ||
        !ReadPod(in, animator.plays_completed) ||
        !ReadBoolByte(in, playback_dirty) ||
        !ReadBoolByte(in, ping_pong_forward)) {
        return false;
    }
    if (playback_mode > static_cast<std::uint8_t>(AnimPlaybackMode::PingPong)) {
        return false;
    }
    animator.animate = animate;
    animator.loop = loop;
    animator.finished = finished;
    animator.playback_mode = static_cast<AnimPlaybackMode>(playback_mode);
    animator.playback_dirty = playback_dirty;
    animator.ping_pong_forward = ping_pong_forward;
    return true;
}

template <typename T>
void WriteOptionalVectorPod(std::ostream& out, const std::optional<std::vector<T>>& values) {
    const std::uint8_t has_value = values.has_value() ? 1U : 0U;
    WritePod(out, has_value);
    if (has_value) {
        WriteVectorPod(out, *values);
    }
}

template <typename T>
bool ReadOptionalVectorPod(std::istream& in, std::optional<std::vector<T>>& values) {
    std::uint8_t has_value = 0;
    if (!ReadPod(in, has_value)) {
        return false;
    }
    if (has_value == 0) {
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
    WriteVid(out, ent.vid);
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
    WriteAFrameAnimator(out, ent.aframe_animator);
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
    WriteOptionalSizeIndex(out, ent.stage_spawn_index);
    WriteOptionalVid(out, ent.back_vid);
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
    WriteOptionalPod(out, ent.transition_target);
    WritePod(out, ent.stage_exit_id);
    WritePod(out, ent.attack_weight);
    WritePod(out, ent.weight);
    WritePod(out, ent.bomb_throw_delay_countdown);
    WritePod(out, ent.rope_throw_delay_countdown);
    WritePod(out, ent.attack_delay_countdown);
    WritePod(out, ent.equip_delay_countdown);
    WriteOptionalVid(out, ent.thrown_by);
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
    WriteOptionalVid(out, ent.holding_vid);
    WriteOptionalVid(out, ent.held_by_vid);
    WritePod(out, ent.holding_timer);
    WriteOptionalVid(out, ent.ent_a);
    WriteOptionalVid(out, ent.ent_b);
    WriteOptionalVid(out, ent.ent_c);
    WriteOptionalVid(out, ent.ent_d);
    WriteOptionalVidVector(out, ent.child_vids);
    WriteOptionalVidVector(out, ent.inside_vids);
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
           ReadVid(in, ent.vid) &&
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
           ReadAFrameAnimator(in, ent.aframe_animator) &&
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
           ReadOptionalSizeIndex(in, ent.stage_spawn_index) &&
           ReadOptionalVid(in, ent.back_vid) &&
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
           ReadOptionalPod(in, ent.transition_target) &&
           ReadPod(in, ent.stage_exit_id) &&
           ReadPod(in, ent.attack_weight) &&
           ReadPod(in, ent.weight) &&
           ReadPod(in, ent.bomb_throw_delay_countdown) &&
           ReadPod(in, ent.rope_throw_delay_countdown) &&
           ReadPod(in, ent.attack_delay_countdown) &&
           ReadPod(in, ent.equip_delay_countdown) &&
           ReadOptionalVid(in, ent.thrown_by) &&
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
           ReadOptionalVid(in, ent.holding_vid) &&
           ReadOptionalVid(in, ent.held_by_vid) &&
           ReadPod(in, ent.holding_timer) &&
           ReadOptionalVid(in, ent.ent_a) &&
           ReadOptionalVid(in, ent.ent_b) &&
           ReadOptionalVid(in, ent.ent_c) &&
           ReadOptionalVid(in, ent.ent_d) &&
           ReadOptionalVidVector(in, ent.child_vids) &&
           ReadOptionalVidVector(in, ent.inside_vids) &&
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
    WriteBoolByte(out, settings.video.fullscreen);
    WriteBoolByte(out, settings.video.vsync);
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
    WriteBoolByte(out, settings.post_process.terrain_lighting);
    WriteBoolByte(out, settings.post_process.terrain_seam_ao);
    WriteBoolByte(out, settings.post_process.terrain_exposure_lighting);
    WriteBoolByte(out, settings.post_process.backwall_lighting);
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
        !ReadBoolByte(in, settings.video.fullscreen) ||
        !ReadBoolByte(in, settings.video.vsync) ||
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
        !ReadBoolByte(in, settings.post_process.terrain_lighting) ||
        !ReadBoolByte(in, settings.post_process.terrain_seam_ao) ||
        !ReadBoolByte(in, settings.post_process.terrain_exposure_lighting) ||
        !ReadBoolByte(in, settings.post_process.backwall_lighting) ||
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
    WriteBoolByte(out, requirement.expected);
}

bool ReadStageExitRequirement(std::istream& in, StageExitRequirement& requirement) {
    return ReadString(in, requirement.flag) &&
           ReadBoolByte(in, requirement.expected);
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
    WriteOptionalSizeIndex(out, spawn.ent_a_spawn_index);
    WriteOptionalSizeIndex(out, spawn.ent_b_spawn_index);
    WriteOptionalSizeIndex(out, spawn.ent_c_spawn_index);
    WriteOptionalSizeIndex(out, spawn.ent_d_spawn_index);
    WriteOptionalSizeIndex(out, spawn.shop_owner_spawn_index);
    WriteBoolByte(out, spawn.buyable);
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
           ReadOptionalSizeIndex(in, spawn.ent_a_spawn_index) &&
           ReadOptionalSizeIndex(in, spawn.ent_b_spawn_index) &&
           ReadOptionalSizeIndex(in, spawn.ent_c_spawn_index) &&
           ReadOptionalSizeIndex(in, spawn.ent_d_spawn_index) &&
           ReadOptionalSizeIndex(in, spawn.shop_owner_spawn_index) &&
           ReadBoolByte(in, spawn.buyable) &&
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

void WriteStageLight(std::ostream& out, const StageLight& light) {
    WriteVid(out, light.vid);
    WritePod(out, light.tile_pos);
    WritePod(out, light.radius);
}

bool ReadStageLight(std::istream& in, StageLight& light) {
    return ReadVid(in, light.vid) &&
           ReadPod(in, light.tile_pos) &&
           ReadPod(in, light.radius);
}

void WriteStageLights(std::ostream& out, const std::vector<StageLight>& lights) {
    const std::uint32_t count = static_cast<std::uint32_t>(lights.size());
    WritePod(out, count);
    for (const StageLight& light : lights) {
        WriteStageLight(out, light);
    }
}

bool ReadStageLights(std::istream& in, std::vector<StageLight>& lights) {
    std::uint32_t count = 0;
    if (!ReadPod(in, count)) {
        return false;
    }
    lights.resize(count);
    for (StageLight& light : lights) {
        if (!ReadStageLight(in, light)) {
            return false;
        }
    }
    return true;
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
    WriteBoolByte(out, stage.border.wrap_x);
    WriteBoolByte(out, stage.border.wrap_y);
    WriteOptionalPod(out, stage.border.void_death_y);
    WriteBoolByte(out, stage.camera_clamp_enabled);
    WritePod(out, stage.camera_clamp_margin);
    WriteBoolByte(out, stage.wrap_transform_active);
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
    WriteStageLights(out, stage.lights);
    WritePod(out, stage.block_anim_id);
    WriteSizeIndex(out, stage.next_light_vid);
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
        !ReadBoolByte(in, stage.border.wrap_x) ||
        !ReadBoolByte(in, stage.border.wrap_y) ||
        !ReadOptionalPod(in, stage.border.void_death_y) ||
        !ReadBoolByte(in, stage.camera_clamp_enabled) ||
        !ReadPod(in, stage.camera_clamp_margin) ||
        !ReadBoolByte(in, stage.wrap_transform_active) ||
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

    return ReadStageLights(in, stage.lights) &&
           ReadPod(in, stage.block_anim_id) &&
           ReadSizeIndex(in, stage.next_light_vid) &&
           ReadPod(in, stage.tile_change_generation);
}

void WriteEntPool(std::ostream& out, const EntPool& ents) {
    const std::uint32_t ent_count = static_cast<std::uint32_t>(ents.ents.size());
    WritePod(out, ent_count);
    for (const Ent& ent : ents.ents) {
        WriteEnt(out, ent);
    }
    WriteSizeIndexVector(out, ents.available_ids);
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

    return ReadSizeIndexVector(in, ents.available_ids);
}

void WritePlayerRegistry(std::ostream& out, const PlayerRegistry& players) {
    const std::uint32_t count = static_cast<std::uint32_t>(players.slots.size());
    WritePod(out, count);
    for (const PlayerSlot& slot : players.slots) {
        WritePod(out, slot.player_id);
        WriteOptionalVid(out, slot.ent_vid);
        WritePlayerConnectionKind(out, slot.connection_kind);
        WriteBoolByte(out, slot.connected);
        WriteBoolByte(out, slot.primary_local);
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
            !ReadOptionalVid(in, slot.ent_vid) ||
            !ReadPlayerConnectionKind(in, slot.connection_kind) ||
            !ReadBoolByte(in, slot.connected) ||
            !ReadBoolByte(in, slot.primary_local) ||
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
    const auto write_contact_cooldowns = [&](const std::vector<ContactCooldownEntry>& entries) {
        const std::uint32_t count = static_cast<std::uint32_t>(entries.size());
        WritePod(out, count);
        for (const ContactCooldownEntry& entry : entries) {
            WriteVid(out, entry.source_vid);
            WriteVid(out, entry.target_vid);
            WritePod(out, entry.expires_on_stage_frame);
        }
    };
    const auto write_interaction_cooldowns =
        [&](const std::vector<InteractionCooldownEntry>& entries) {
            const std::uint32_t count = static_cast<std::uint32_t>(entries.size());
            WritePod(out, count);
            for (const InteractionCooldownEntry& entry : entries) {
                WriteVid(out, entry.source_vid);
                WriteVid(out, entry.target_vid);
                WritePod(out, static_cast<std::uint8_t>(entry.kind));
                WritePod(out, entry.expires_on_stage_frame);
            }
        };
    const auto write_ent_dispatches = [&](const std::vector<EntContactDispatchEntry>& entries) {
        const std::uint32_t count = static_cast<std::uint32_t>(entries.size());
        WritePod(out, count);
        for (const EntContactDispatchEntry& entry : entries) {
            WriteVid(out, entry.first_vid);
            WriteVid(out, entry.second_vid);
        }
    };
    const auto write_proj_body_cooldowns =
        [&](const std::vector<ProjBodyImpactCooldownEntry>& entries) {
            const std::uint32_t count = static_cast<std::uint32_t>(entries.size());
            WritePod(out, count);
            for (const ProjBodyImpactCooldownEntry& entry : entries) {
                WriteVid(out, entry.first_vid);
                WriteVid(out, entry.second_vid);
                WritePod(out, entry.expires_on_stage_frame);
            }
        };

    write_contact_cooldowns(contact.contact_cooldowns);
    write_interaction_cooldowns(contact.interaction_cooldowns);
    write_ent_dispatches(contact.ent_contact_dispatches_this_tick);
    write_proj_body_cooldowns(contact.proj_body_impact_cooldowns);
}

bool ReadContactBookkeeping(std::istream& in, ContactBookkeeping& contact) {
    const auto read_count = [&](std::uint32_t& count) {
        return ReadPod(in, count);
    };
    const auto read_contact_cooldowns = [&]() {
        std::uint32_t count = 0;
        if (!read_count(count)) {
            return false;
        }
        contact.contact_cooldowns.resize(count);
        for (ContactCooldownEntry& entry : contact.contact_cooldowns) {
            if (!ReadVid(in, entry.source_vid) ||
                !ReadVid(in, entry.target_vid) ||
                !ReadPod(in, entry.expires_on_stage_frame)) {
                return false;
            }
        }
        return true;
    };
    const auto read_interaction_cooldowns = [&]() {
        std::uint32_t count = 0;
        if (!read_count(count)) {
            return false;
        }
        contact.interaction_cooldowns.resize(count);
        for (InteractionCooldownEntry& entry : contact.interaction_cooldowns) {
            std::uint8_t kind = 0;
            if (!ReadVid(in, entry.source_vid) ||
                !ReadVid(in, entry.target_vid) ||
                !ReadPod(in, kind) ||
                !ReadPod(in, entry.expires_on_stage_frame)) {
                return false;
            }
            entry.kind = static_cast<InteractionCooldownKind>(kind);
        }
        return true;
    };
    const auto read_ent_dispatches = [&]() {
        std::uint32_t count = 0;
        if (!read_count(count)) {
            return false;
        }
        contact.ent_contact_dispatches_this_tick.resize(count);
        for (EntContactDispatchEntry& entry : contact.ent_contact_dispatches_this_tick) {
            if (!ReadVid(in, entry.first_vid) ||
                !ReadVid(in, entry.second_vid)) {
                return false;
            }
        }
        return true;
    };
    const auto read_proj_body_cooldowns = [&]() {
        std::uint32_t count = 0;
        if (!read_count(count)) {
            return false;
        }
        contact.proj_body_impact_cooldowns.resize(count);
        for (ProjBodyImpactCooldownEntry& entry : contact.proj_body_impact_cooldowns) {
            if (!ReadVid(in, entry.first_vid) ||
                !ReadVid(in, entry.second_vid) ||
                !ReadPod(in, entry.expires_on_stage_frame)) {
                return false;
            }
        }
        return true;
    };

    return read_contact_cooldowns() &&
           read_interaction_cooldowns() &&
           read_ent_dispatches() &&
           read_proj_body_cooldowns();
}

void WriteToolSlot(std::ostream& out, const ToolSlot& slot) {
    const std::uint8_t kind = static_cast<std::uint8_t>(slot.kind);
    WritePod(out, kind);
    WritePod(out, slot.count);
    WritePod(out, slot.cooldown);
    WriteBoolByte(out, slot.active);
}

bool ReadToolSlot(std::istream& in, ToolSlot& slot) {
    std::uint8_t kind = 0;
    std::uint16_t count = 0;
    std::uint16_t cooldown = 0;
    bool active = false;
    if (!ReadPod(in, kind) ||
        !ReadPod(in, count) ||
        !ReadPod(in, cooldown) ||
        !ReadBoolByte(in, active)) {
        return false;
    }
    if (kind >= static_cast<std::uint8_t>(ToolKind::ThrowStickyBomb) + 1U) {
        return false;
    }
    slot.kind = static_cast<ToolKind>(kind);
    slot.count = count;
    slot.cooldown = cooldown;
    slot.active = active;
    return true;
}

void WriteEntToolState(std::ostream& out, const EntToolState& state) {
    WriteVid(out, state.owner_vid);
    for (const ToolSlot& slot : state.slots) {
        WriteToolSlot(out, slot);
    }
}

bool ReadEntToolState(std::istream& in, EntToolState& state) {
    if (!ReadVid(in, state.owner_vid)) {
        return false;
    }
    for (ToolSlot& slot : state.slots) {
        if (!ReadToolSlot(in, slot)) {
            return false;
        }
    }
    return true;
}

void WriteEntToolStates(std::ostream& out, const std::vector<EntToolState>& states) {
    const std::uint32_t count = static_cast<std::uint32_t>(states.size());
    WritePod(out, count);
    for (const EntToolState& state : states) {
        WriteEntToolState(out, state);
    }
}

bool ReadEntToolStates(std::istream& in, std::vector<EntToolState>& states) {
    std::uint32_t count = 0;
    if (!ReadPod(in, count)) {
        return false;
    }
    states.resize(count);
    for (EntToolState& state : states) {
        if (!ReadEntToolState(in, state)) {
            return false;
        }
    }
    return true;
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
    WriteOptionalSizeIndex(out, snapshot.video_settings_target_window_size_index);
    WriteOptionalSizeIndex(out, snapshot.video_settings_target_resolution_index);
    WriteOptionalBoolByte(out, snapshot.video_settings_target_fullscreen);
    WriteBoolByte(out, snapshot.rebuild_render_texture);
    WriteBoolByte(out, snapshot.choosing_control_binding);
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
    WriteBoolByte(out, snapshot.game_over);
    WriteBoolByte(out, snapshot.pause);
    WriteBoolByte(out, snapshot.win);
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
    WriteOptionalVid(out, snapshot.controlled_ent_vid);
    WriteOptionalPod(out, snapshot.spectator_target_player_id);
    WriteOptionalVid(out, snapshot.mouse_trailer_vid);
    WriteContactBookkeeping(out, snapshot.contact);
    WriteEntToolStates(out, snapshot.ent_tool_states);
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
           ReadOptionalSizeIndex(in, snapshot.video_settings_target_window_size_index) &&
           ReadOptionalSizeIndex(in, snapshot.video_settings_target_resolution_index) &&
           ReadOptionalBoolByte(in, snapshot.video_settings_target_fullscreen) &&
           ReadBoolByte(in, snapshot.rebuild_render_texture) &&
           ReadBoolByte(in, snapshot.choosing_control_binding) &&
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
           ReadBoolByte(in, snapshot.game_over) &&
           ReadBoolByte(in, snapshot.pause) &&
           ReadBoolByte(in, snapshot.win) &&
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
           ReadOptionalVid(in, snapshot.controlled_ent_vid) &&
           ReadOptionalPod(in, snapshot.spectator_target_player_id) &&
           ReadOptionalVid(in, snapshot.mouse_trailer_vid) &&
           ReadContactBookkeeping(in, snapshot.contact) &&
           ReadEntToolStates(in, snapshot.ent_tool_states) &&
           ReadPod(in, snapshot.play_cam_pos);
}

void WriteSimPlayerSlotSnapshot(std::ostream& out, const SimPlayerSlotSnapshot& slot) {
    WritePod(out, slot.player_id);
    WriteOptionalVid(out, slot.ent_vid);
    WriteBoolByte(out, slot.connected);
    WriteString(out, slot.display_name);
    WritePod(out, slot.input_frame);
    WritePod(out, slot.previous_input_frame);
    WritePod(out, slot.inputs);
    WritePod(out, slot.immediate_inputs);
}

bool ReadSimPlayerSlotSnapshot(std::istream& in, SimPlayerSlotSnapshot& slot) {
    return ReadPod(in, slot.player_id) &&
           ReadOptionalVid(in, slot.ent_vid) &&
           ReadBoolByte(in, slot.connected) &&
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
    const std::uint32_t count = static_cast<std::uint32_t>(links.size());
    WritePod(out, count);
    for (const SimNetEntLinkSnapshot& link : links) {
        WritePod(out, link.net_id);
        WriteVid(out, link.local_vid);
        WriteBoolByte(out, link.has_input_owner);
        WritePod(out, link.input_owner_player_id);
    }
}

bool ReadSimNetEntLinks(
    std::istream& in,
    std::vector<SimNetEntLinkSnapshot>& links
) {
    std::uint32_t count = 0;
    if (!ReadPod(in, count)) {
        return false;
    }
    links.resize(count);
    for (SimNetEntLinkSnapshot& link : links) {
        if (!ReadPod(in, link.net_id) ||
            !ReadVid(in, link.local_vid) ||
            !ReadBoolByte(in, link.has_input_owner) ||
            !ReadPod(in, link.input_owner_player_id)) {
            return false;
        }
    }
    return true;
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
    WriteBoolByte(out, snapshot.running);
    WritePod(out, snapshot.now);
    WritePod(out, snapshot.time_since_last_update);
    WritePod(out, snapshot.scene_frame);
    WritePod(out, snapshot.frame);
    WritePod(out, snapshot.stage_frame);
    WritePod(out, snapshot.drng);
    WritePod(out, snapshot.stagegen_drng);
    WritePod(out, snapshot.menu_return_to);
    WriteBoolByte(out, snapshot.game_over);
    WriteBoolByte(out, snapshot.pause);
    WriteBoolByte(out, snapshot.win);
    WritePod(out, snapshot.respawn_target);
    WriteOptionalPod(out, snapshot.pending_stage_transition);
    WritePod(out, snapshot.multiplayer_respawn_mode);
    WritePod(out, snapshot.points);
    WritePod(out, snapshot.deaths);
    WritePod(out, snapshot.depth);
    WritePod(out, snapshot.sac_altar_favor);
    WritePod(out, snapshot.sac_altar_reward_tier);
    WriteVidVector(out, snapshot.interact_claimed_vids_this_frame);
    WritePod(out, snapshot.quest_state);
    WriteSimPlayerSlots(out, snapshot.players);
    WritePod(out, snapshot.frame_pause);
    WritePod(out, snapshot.debug_level);
    WriteEntPool(out, snapshot.ents);
    WriteVidVector(out, snapshot.area_listener_vids);
    WriteStage(out, snapshot.stage);
    WriteContactBookkeeping(out, snapshot.contact);
    WriteEntToolStates(out, snapshot.ent_tool_states);
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
           ReadBoolByte(in, snapshot.running) &&
           ReadPod(in, snapshot.now) &&
           ReadPod(in, snapshot.time_since_last_update) &&
           ReadPod(in, snapshot.scene_frame) &&
           ReadPod(in, snapshot.frame) &&
           ReadPod(in, snapshot.stage_frame) &&
           ReadPod(in, snapshot.drng) &&
           ReadPod(in, snapshot.stagegen_drng) &&
           ReadPod(in, snapshot.menu_return_to) &&
           ReadBoolByte(in, snapshot.game_over) &&
           ReadBoolByte(in, snapshot.pause) &&
           ReadBoolByte(in, snapshot.win) &&
           ReadPod(in, snapshot.respawn_target) &&
           ReadOptionalPod(in, snapshot.pending_stage_transition) &&
           ReadPod(in, snapshot.multiplayer_respawn_mode) &&
           ReadPod(in, snapshot.points) &&
           ReadPod(in, snapshot.deaths) &&
           ReadPod(in, snapshot.depth) &&
           ReadPod(in, snapshot.sac_altar_favor) &&
           ReadPod(in, snapshot.sac_altar_reward_tier) &&
           ReadVidVector(in, snapshot.interact_claimed_vids_this_frame) &&
           ReadPod(in, snapshot.quest_state) &&
           ReadSimPlayerSlots(in, snapshot.players) &&
           ReadPod(in, snapshot.frame_pause) &&
           ReadPod(in, snapshot.debug_level) &&
           ReadEntPool(in, snapshot.ents) &&
           ReadVidVector(in, snapshot.area_listener_vids) &&
           ReadStage(in, snapshot.stage) &&
           ReadContactBookkeeping(in, snapshot.contact) &&
           ReadEntToolStates(in, snapshot.ent_tool_states) &&
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

#include "ent/spec.hpp"

#include "aframe_id.hpp"

namespace splonks {

void SetEntAs(Ent& ent, EntType type_) {
    const VID existing_vid = ent.vid;
    ent.Reset();
    const EntSpec& spec = GetEntSpec(type_);
    ent.type_ = spec.type_;
    ent.has_physics = spec.has_physics;
    ent.can_collide = spec.can_collide;
    ent.can_be_hit = spec.can_be_hit;
    ent.can_receive_proj_contact = spec.can_receive_proj_contact;
    ent.can_be_picked_up = spec.can_be_picked_up;
    ent.affected_by_cobweb = spec.affected_by_cobweb;
    ent.can_collect_pickups = spec.can_collect_pickups;
    ent.can_only_be_picked_up_if_dead_or_stunned =
        spec.can_only_be_picked_up_if_dead_or_stunned;
    ent.impassable = spec.impassable;
    ent.can_be_hung_on = spec.can_be_hung_on;
    ent.hurt_on_contact = spec.hurt_on_contact;
    ent.can_stomp = spec.can_stomp;
    ent.can_be_stomped = spec.can_be_stomped;
    ent.vanish_on_death = spec.vanish_on_death;
    ent.affected_by_ground_friction = spec.affected_by_ground_friction;
    ent.support_ground_friction = spec.support_ground_friction;
    ent.pushable = spec.pushable;
    ent.push_acc = spec.push_acc;
    ent.jump_hold_gravity_frames_remaining = 0;
    ent.throw_velocity_scale = spec.throw_velocity_scale;
    ent.buoyancy = spec.buoyancy;
    ent.alpha = spec.alpha;
    ent.self_light = spec.self_light;
    ent.light_strength = spec.light_strength;
    ent.light_color = spec.light_color;
    ent.light_radius = spec.light_radius;
    ent.pickup_effect = spec.pickup_effect;
    ent.buyable = spec.buyable;
    ent.damage_anim = spec.damage_anim;
    ent.damage_sound = spec.damage_sound;
    ent.collide_sound = spec.collide_sound;
    ent.death_sound = spec.death_sound;
    ent.on_death = spec.on_death;
    ent.on_damage = spec.on_damage;
    ent.on_use = spec.on_use;
    ent.on_area_enter = spec.on_area_enter;
    ent.on_area_exit = spec.on_area_exit;
    ent.on_area_tile_changed = spec.on_area_tile_changed;
    ent.control_logic = spec.control_logic;
    ent.step_logic = spec.step_logic;
    ent.step_physics = spec.step_physics;
    ent.crusher_pusher = spec.crusher_pusher;
    ent.can_go_on_back = spec.can_go_on_back;
    ent.can_hang_ledge = spec.can_hang_ledge;
    ent.can_be_stunned = spec.can_be_stunned;
    ent.stun_recovers_on_ground = spec.stun_recovers_on_ground;
    ent.stun_recovers_while_held = spec.stun_recovers_while_held;
    ent.size = spec.size;
    ent.facing = spec.facing;
    ent.draw_layer = spec.draw_layer;
    ent.render_enabled = spec.render_enabled;
    TrySetAnim(ent, spec.display_state);
    ent.condition = spec.condition;
    ent.ai_state = spec.ai_state;
    ent.health = spec.health;
    ent.counter_a = spec.counter_a;
    ent.counter_b = spec.counter_b;
    ent.counter_d = spec.counter_d;
    ent.damage_vuln = spec.damage_vuln;
    ent.proj_contact_damage_type = spec.proj_contact_damage_type;
    ent.proj_contact_damage_amount = spec.proj_contact_damage_amount;
    ent.can_apply_proj_contact = spec.can_apply_proj_contact;
    ent.ent_label_a = spec.ent_label_a;
    ent.alignment = spec.alignment;
    ent.aframe_animator = spec.aframe_animator;
    ent.vid = existing_vid;
}

AFrameId GetDefaultAnimIdForSpec(EntType type_) {
    const AFrameId anim_id = GetEntSpec(type_).aframe_animator.anim_id;
    if (anim_id == kInvalidAFrameId) {
        return aframe_ids::NoSprite;
    }
    return anim_id;
}

} // namespace splonks

#include "ent/spec_restore.hpp"

#include "ent/spec.hpp"

namespace splonks {

namespace {

const EntSpec& GetSpecForEnt(const Ent& ent) {
    return GetEntSpec(ent.type_);
}

} // namespace

void RestoreEntDetachedCarryStateFromSpec(Ent& ent) {
    const EntSpec& spec = GetSpecForEnt(ent);
    ent.has_physics = spec.has_physics;
    ent.can_collide = spec.can_collide;
    ent.draw_layer = spec.draw_layer;
}

void RestoreEntStageEntryStateFromSpec(Ent& ent) {
    const EntSpec& spec = GetSpecForEnt(ent);
    ent.condition = spec.condition;
    ent.size = spec.size;
    ent.has_physics = spec.has_physics;
    ent.can_collide = spec.can_collide;
    ent.draw_layer = spec.draw_layer;
    ent.render_enabled = spec.render_enabled;
    ent.aframe_animator = spec.aframe_animator;
}

void RestoreEntStoneStateFromSpec(Ent& ent) {
    const EntSpec& spec = GetSpecForEnt(ent);
    ent.crusher_pusher = spec.crusher_pusher;
    ent.impassable = spec.impassable;
    ent.damage_vuln = spec.damage_vuln;
}

void RestoreEntRuntimeCallbacksFromSpec(Ent& ent) {
    const EntSpec& spec = GetSpecForEnt(ent);
    ent.on_death = spec.on_death;
    ent.on_damage = spec.on_damage;
    ent.on_use = spec.on_use;
    ent.on_area_enter = spec.on_area_enter;
    ent.on_area_exit = spec.on_area_exit;
    ent.on_area_tile_changed = spec.on_area_tile_changed;
    ent.control_logic = spec.control_logic;
    ent.step_logic = spec.step_logic;
    ent.step_physics = spec.step_physics;
}

} // namespace splonks

#include "ents/common/common.hpp"

#include "controls.hpp"
#include "ent/spec.hpp"
#include "sim/fxp.hpp"
#include "world_ops.hpp"

namespace splonks::ents::common {

namespace {

Vec2 BuildThrowVelocity(const controls::ControlIntent& control) {
    Vec2 throw_vel = Vec2::New(0.0F, 0.0F);
    if (control.left) {
        throw_vel.x = -10.0F;
    } else if (control.right) {
        throw_vel.x = 10.0F;
    }
    if (control.up) {
        throw_vel.y = -10.0F;
    }
    if (control.down) {
        throw_vel.y = 10.0F;
    }
    if (!control.up && !control.down && (control.left || control.right)) {
        throw_vel.y = -2.0F;
    }
    return throw_vel;
}

} // namespace

bool TrySpawnAndThrowEntForToolUse(
    std::size_t thrower_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    ToolSlot& tool_slot,
    bool trigger_pressed,
    std::uint16_t cooldown_frames,
    std::uint32_t thrown_immunity_timer,
    void (*setup_ent)(Ent&),
    ToolThrowVelocityBuilder build_throw_velocity,
    std::optional<Vec2> throw_velocity_override
) {
    (void)audio;
    if (!trigger_pressed) {
        return false;
    }

    Ent& thrower = state.ents.ents[thrower_idx];
    if (!tool_slot.active || tool_slot.count == 0 || tool_slot.cooldown > 0) {
        return false;
    }

    const controls::ControlIntent control =
        controls::GetControlIntentForEnt(thrower, state);
    const ToolThrowVelocityBuilder velocity_builder =
        build_throw_velocity == nullptr ? BuildThrowVelocity : build_throw_velocity;
    Ent* const spawned_ent = world_ops::SpawnConfiguredEnt(state, [&](Ent& spawned) {
        setup_ent(spawned);
        spawned.has_physics = true;
        spawned.can_collide = true;
        UseEnt(spawned, thrower.vid, AttachMode::None);
        spawned.thrown_by = thrower.vid;
        spawned.thrown_immunity_timer = thrown_immunity_timer;
        const EntSpec& spawned_spec = GetEntSpec(spawned.type_);
        spawned.can_apply_proj_contact = spawned_spec.can_apply_proj_contact;
        spawned.proj_contact_damage_type = spawned_spec.proj_contact_damage_type;
        spawned.proj_contact_damage_amount = spawned_spec.proj_contact_damage_amount;
        spawned.proj_contact_timer = kProjContactDuration;
        spawned.SetCenter(thrower.GetCenter());
        const Vec2 throw_velocity =
            throw_velocity_override.value_or(
                velocity_builder(control) * sim::ToRenderScalar(thrower.throw_velocity_scale)
            );
        spawned.acc += sim::ToSimVec2(throw_velocity);
        if (spawned.on_use != nullptr) {
            spawned.on_use(spawned.vid.id, state, graphics, audio);
        }
        state.UpdateSidForEnt(spawned.vid.id, graphics);
    });
    if (spawned_ent == nullptr) {
        return false;
    }

    tool_slot.count -= 1;
    tool_slot.cooldown = cooldown_frames;
    (void)PlayEntCenterSoundEmitter(state, thrower, audio_asset_ids::Throw);
    return true;
}

} // namespace splonks::ents::common

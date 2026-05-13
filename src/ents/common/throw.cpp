#include "entities/common/common.hpp"

#include "controls.hpp"
#include "entity/archetype.hpp"
#include "world_ops.hpp"

namespace splonks::entities::common {

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

bool TrySpawnAndThrowEntityForToolUse(
    std::size_t thrower_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    ToolSlot& tool_slot,
    bool trigger_pressed,
    std::uint16_t cooldown_frames,
    std::uint32_t thrown_immunity_timer,
    void (*setup_entity)(Entity&),
    ToolThrowVelocityBuilder build_throw_velocity,
    std::optional<Vec2> throw_velocity_override
) {
    (void)audio;
    if (!trigger_pressed) {
        return false;
    }

    Entity& thrower = state.entity_manager.entities[thrower_idx];
    if (!tool_slot.active || tool_slot.count == 0 || tool_slot.cooldown > 0) {
        return false;
    }

    const controls::ControlIntent control =
        controls::GetControlIntentForEntity(thrower, state);
    const ToolThrowVelocityBuilder velocity_builder =
        build_throw_velocity == nullptr ? BuildThrowVelocity : build_throw_velocity;
    Entity* const spawned_entity = world_ops::SpawnConfiguredEntity(state, [&](Entity& spawned) {
        setup_entity(spawned);
        spawned.has_physics = true;
        spawned.can_collide = true;
        UseEntity(spawned, thrower.vid, AttachmentMode::None);
        spawned.thrown_by = thrower.vid;
        spawned.thrown_immunity_timer = thrown_immunity_timer;
        const EntityArchetype& spawned_archetype = GetEntityArchetype(spawned.type_);
        spawned.can_apply_projectile_contact = spawned_archetype.can_apply_projectile_contact;
        spawned.projectile_contact_damage_type = spawned_archetype.projectile_contact_damage_type;
        spawned.projectile_contact_damage_amount = spawned_archetype.projectile_contact_damage_amount;
        spawned.projectile_contact_timer = kProjectileContactDuration;
        spawned.SetCenter(thrower.GetCenter());
        const Vec2 throw_velocity =
            throw_velocity_override.value_or(velocity_builder(control) * thrower.throw_velocity_scale);
        spawned.acc += throw_velocity;
        if (spawned.on_use != nullptr) {
            spawned.on_use(spawned.vid.id, state, graphics, audio);
        }
        state.UpdateSidForEntity(spawned.vid.id, graphics);
    });
    if (spawned_entity == nullptr) {
        return false;
    }

    tool_slot.count -= 1;
    tool_slot.cooldown = cooldown_frames;
    (void)PlayEntityCenterSoundEmitter(state, thrower, audio_asset_ids::Throw);
    return true;
}

} // namespace splonks::entities::common

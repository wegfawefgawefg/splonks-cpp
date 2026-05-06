#include "entities/common/common.hpp"

#include "controls.hpp"
#include "entity/archetype.hpp"
#include "gameplay_events.hpp"

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

    const Entity& thrower = state.entity_manager.entities[thrower_idx];
    if (!tool_slot.active || tool_slot.count == 0 || tool_slot.cooldown > 0) {
        return false;
    }

    const controls::ControlIntent control =
        controls::GetControlIntentForEntity(thrower, state);
    const std::optional<VID> vid = state.entity_manager.NewEntity();
    if (!vid.has_value()) {
        return false;
    }

    Entity* const spawned_entity = state.entity_manager.GetEntityMut(*vid);
    if (spawned_entity == nullptr) {
        return false;
    }

    setup_entity(*spawned_entity);
    spawned_entity->has_physics = true;
    spawned_entity->can_collide = true;
    UseEntity(*spawned_entity, thrower.vid, AttachmentMode::None);
    spawned_entity->thrown_by = thrower.vid;
    spawned_entity->thrown_immunity_timer = thrown_immunity_timer;
    const EntityArchetype& spawned_archetype = GetEntityArchetype(spawned_entity->type_);
    spawned_entity->can_apply_projectile_contact = spawned_archetype.can_apply_projectile_contact;
    spawned_entity->projectile_contact_damage_type = spawned_archetype.projectile_contact_damage_type;
    spawned_entity->projectile_contact_damage_amount = spawned_archetype.projectile_contact_damage_amount;
    spawned_entity->projectile_contact_timer = kProjectileContactDuration;
    const ToolThrowVelocityBuilder velocity_builder =
        build_throw_velocity == nullptr ? BuildThrowVelocity : build_throw_velocity;
    spawned_entity->SetCenter(thrower.GetCenter());
    const Vec2 throw_velocity =
        throw_velocity_override.value_or(velocity_builder(control) * thrower.throw_velocity_scale);
    spawned_entity->acc += throw_velocity;
    if (spawned_entity->on_use != nullptr) {
        spawned_entity->on_use(vid->id, state, graphics, audio);
    }
    state.UpdateSidForEntity(vid->id, graphics);
    EmitEntitySpawnedGameplayEvent(state, *spawned_entity);

    tool_slot.count -= 1;
    tool_slot.cooldown = cooldown_frames;
    (void)PlayEntityCenterSoundEmitter(state, thrower, audio_asset_ids::Throw);
    return true;
}

} // namespace splonks::entities::common

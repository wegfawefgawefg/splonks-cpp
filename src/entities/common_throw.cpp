#include "entities/common.hpp"

#include "systems/controls.hpp"

namespace splonks::entities::common {

namespace {

Vec2 BuildThrowVelocity(const systems::controls::ControlIntent& control) {
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

bool TrySpawnAndThrowEntityFromTool(
    std::size_t thrower_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    std::size_t tool_slot_index,
    bool trigger_pressed,
    std::uint16_t cooldown_frames,
    std::uint32_t thrown_immunity_timer,
    void (*setup_entity)(Entity&),
    ToolThrowVelocityBuilder build_throw_velocity
) {
    if (!trigger_pressed) {
        return false;
    }

    const Entity& thrower = state.entity_manager.entities[thrower_idx];
    ToolSlot* const tool_slot = state.FindToolSlotMut(thrower.vid, tool_slot_index);
    if (tool_slot == nullptr || !tool_slot->active || tool_slot->count == 0 ||
        tool_slot->cooldown > 0) {
        return false;
    }

    const systems::controls::ControlIntent control =
        systems::controls::GetControlIntentForEntity(thrower, state);
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
    spawned_entity->state = EntityState::InUse;
    spawned_entity->thrown_by = thrower.vid;
    spawned_entity->thrown_immunity_timer = thrown_immunity_timer;
    const ToolThrowVelocityBuilder velocity_builder =
        build_throw_velocity == nullptr ? BuildThrowVelocity : build_throw_velocity;
    spawned_entity->SetCenter(thrower.GetCenter());
    spawned_entity->acc += velocity_builder(control);
    state.UpdateSidForEntity(vid->id, graphics);

    tool_slot->count -= 1;
    tool_slot->cooldown = cooldown_frames;
    audio.PlaySoundEffect(SoundEffect::Throw);
    return true;
}

} // namespace splonks::entities::common

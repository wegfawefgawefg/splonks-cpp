#include "entities/common/common.hpp"

#include "controls.hpp"
#include "gameplay_authority.hpp"
#include "gameplay_events.hpp"
#include "tools/tool_archetype.hpp"

namespace splonks::entities::common {

namespace {

Vec2 BuildDefaultToolThrowVelocity(const controls::ControlIntent& control) {
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

bool TryUseToolSlot(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    std::size_t tool_slot_index,
    bool trigger_pressed,
    ToolThrowVelocityBuilder build_throw_velocity,
    std::optional<Vec2> throw_velocity_override
) {
    const Entity& entity = state.entity_manager.entities[entity_idx];
    const ToolSlot* const tool_slot = state.entity_tools.FindToolSlot(entity.vid, tool_slot_index);
    if (tool_slot == nullptr || !tool_slot->active) {
        return false;
    }

    const ToolArchetype& tool_archetype = GetToolArchetype(tool_slot->kind);
    if (tool_archetype.use_fn == nullptr) {
        return false;
    }
    if (state.net_session.role == network::NetRole::Peer &&
        trigger_pressed &&
        tool_slot->count > 0 &&
        tool_slot->cooldown == 0 &&
        HasLocalGameplayAuthorityForInteractionSource(state, entity.vid)) {
        const controls::ControlIntent control = controls::GetControlIntentForEntity(entity, state);
        const ToolThrowVelocityBuilder velocity_builder =
            build_throw_velocity == nullptr ? BuildDefaultToolThrowVelocity : build_throw_velocity;
        EmitGameplayActionRequested(
            state,
            GameplayActionRequested{
                .kind = GameplayActionKind::UseTool,
                .source_vid = entity.vid,
                .velocity = throw_velocity_override.value_or(
                    velocity_builder(control) * entity.throw_velocity_scale
                ),
                .param_a = static_cast<std::uint32_t>(tool_slot_index),
            }
        );
        return true;
    }

    return tool_archetype.use_fn(
        entity_idx,
        state,
        graphics,
        audio,
        tool_slot_index,
        trigger_pressed,
        build_throw_velocity,
        throw_velocity_override
    );
}

} // namespace splonks::entities::common

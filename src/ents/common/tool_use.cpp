#include "ents/common/common.hpp"

#include "tools/tool_spec.hpp"

namespace splonks::ents::common {

bool TryUseToolSlot(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    std::size_t tool_slot_index,
    bool trigger_pressed,
    ToolThrowVelocityBuilder build_throw_velocity,
    std::optional<Vec2> throw_velocity_override
) {
    const Ent& ent = state.ents.ents[ent_idx];
    const ToolSlot* const tool_slot = state.ent_tools.FindToolSlot(ent.vid, tool_slot_index);
    if (tool_slot == nullptr || !tool_slot->active) {
        return false;
    }

    const ToolSpec& tool_spec = GetToolSpec(tool_slot->kind);
    if (tool_spec.use_fn == nullptr) {
        return false;
    }

    return tool_spec.use_fn(
        ent_idx,
        state,
        graphics,
        audio,
        tool_slot_index,
        trigger_pressed,
        build_throw_velocity,
        throw_velocity_override
    );
}

} // namespace splonks::ents::common

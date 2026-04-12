#include "entities/common.hpp"

#include "tools/tool_archetype.hpp"

namespace splonks::entities::common {

bool TryUseToolSlot(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    std::size_t tool_slot_index,
    bool trigger_pressed
) {
    const Entity& entity = state.entity_manager.entities[entity_idx];
    const ToolSlot* const tool_slot = state.FindToolSlot(entity.vid, tool_slot_index);
    if (tool_slot == nullptr || !tool_slot->active) {
        return false;
    }

    const ToolArchetype& tool_archetype = GetToolArchetype(tool_slot->kind);
    if (tool_archetype.use_fn == nullptr) {
        return false;
    }
    return tool_archetype.use_fn(
        entity_idx,
        state,
        graphics,
        audio,
        tool_slot_index,
        trigger_pressed
    );
}

} // namespace splonks::entities::common

#include "tools/throw_rope.hpp"

#include "entities/common.hpp"
#include "entity_archetype.hpp"
#include "state.hpp"

namespace splonks::tools::throw_rope {

namespace {

bool UseThrowRopeTool(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    std::size_t tool_slot_index,
    bool trigger_pressed
) {
    const VID user_vid = state.entity_manager.entities[entity_idx].vid;
    ToolSlot* const tool_slot = state.FindToolSlotMut(user_vid, tool_slot_index);
    if (tool_slot == nullptr) {
        return false;
    }

    return entities::common::TrySpawnAndThrowEntityForToolUse(
        entity_idx,
        state,
        graphics,
        audio,
        *tool_slot,
        trigger_pressed,
        kThrowRopeToolArchetype.use_cooldown_frames,
        entities::common::kThrownByImmunityDuration * 2,
        [](Entity& spawned_entity) { SetEntityAs(spawned_entity, EntityType::Rope); }
    );
}

} // namespace

extern const ToolArchetype kThrowRopeToolArchetype{
    .kind = ToolKind::ThrowRope,
    .debug_name = "ThrowRope",
    .icon_animation_id = frame_data_ids::RopeUiIcon,
    .use_cooldown_frames = 8,
    .preferred_slot_index = 1,
    .use_fn = UseThrowRopeTool,
};

} // namespace splonks::tools::throw_rope

#include "tools/throw_bomb.hpp"

#include "audio.hpp"
#include "entities/common/common.hpp"
#include "entity.hpp"
#include "entity/archetype.hpp"
#include "entity/core_types.hpp"
#include "entity/manager.hpp"
#include "frame_data_id.hpp"
#include "graphics.hpp"
#include "state.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace splonks::tools::throw_bomb {

namespace {

bool UseThrowBombTool(
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
        kThrowBombToolArchetype.use_cooldown_frames,
        entities::common::kThrownByImmunityDuration,
        [](Entity& spawned_entity) { SetEntityAs(spawned_entity, EntityType::Bomb); }
    );
}

} // namespace

extern const ToolArchetype kThrowBombToolArchetype{
    .kind = ToolKind::ThrowBomb,
    .debug_name = "ThrowBomb",
    .icon_animation_id = frame_data_ids::GrenadeUiIcon,
    .use_cooldown_frames = 8,
    .preferred_slot_index = 0,
    .use_fn = UseThrowBombTool,
};

} // namespace splonks::tools::throw_bomb

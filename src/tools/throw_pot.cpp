#include "tools/throw_pot.hpp"

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

namespace splonks::tools::throw_pot {

namespace {

bool UseThrowPotTool(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    std::size_t tool_slot_index,
    bool trigger_pressed
) {
    const VID user_vid = state.entity_manager.entities[entity_idx].vid;
    ToolSlot* const tool_slot = state.entity_tools.FindToolSlotMut(user_vid, tool_slot_index);
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
        kThrowPotToolArchetype.use_cooldown_frames,
        entities::common::kThrownByImmunityDuration,
        [](Entity& spawned_entity) { SetEntityAs(spawned_entity, EntityType::Pot); }
    );
}

} // namespace

extern const ToolArchetype kThrowPotToolArchetype{
    .kind = ToolKind::ThrowPot,
    .debug_name = "ThrowPot",
    .icon_animation_id = frame_data_ids::Pot,
    .use_cooldown_frames = 12,
    .use_fn = UseThrowPotTool,
};

} // namespace splonks::tools::throw_pot

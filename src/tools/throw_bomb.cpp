#include "tools/throw_bomb.hpp"

#include "audio.hpp"
#include "entities/bomb.hpp"
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
    bool trigger_pressed,
    ToolThrowVelocityBuilder build_throw_velocity,
    std::optional<Vec2> throw_velocity_override
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
        kThrowBombToolArchetype.use_cooldown_frames,
        entities::common::kThrownByImmunityDuration,
        [](Entity& spawned_entity) { SetEntityAs(spawned_entity, EntityType::Bomb); },
        build_throw_velocity,
        throw_velocity_override
    );
}

bool UseThrowStickyBombTool(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    std::size_t tool_slot_index,
    bool trigger_pressed,
    ToolThrowVelocityBuilder build_throw_velocity,
    std::optional<Vec2> throw_velocity_override
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
        kThrowStickyBombToolArchetype.use_cooldown_frames,
        entities::common::kThrownByImmunityDuration,
        [](Entity& spawned_entity) {
            SetEntityAs(spawned_entity, EntityType::Bomb);
            entities::bomb::MarkBombSticky(spawned_entity);
        },
        build_throw_velocity,
        throw_velocity_override
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

extern const ToolArchetype kThrowStickyBombToolArchetype{
    .kind = ToolKind::ThrowStickyBomb,
    .debug_name = "ThrowStickyBomb",
    .icon_animation_id = frame_data_ids::StickyGrenadeUiIcon,
    .use_cooldown_frames = 8,
    .preferred_slot_index = 0,
    .use_fn = UseThrowStickyBombTool,
};

} // namespace splonks::tools::throw_bomb

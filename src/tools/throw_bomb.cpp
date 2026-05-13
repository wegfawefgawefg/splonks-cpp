#include "tools/throw_bomb.hpp"

#include "audio.hpp"
#include "ents/bomb.hpp"
#include "ents/common/common.hpp"
#include "ent.hpp"
#include "ent/spec.hpp"
#include "ent/core_types.hpp"
#include "ent/manager.hpp"
#include "aframe_id.hpp"
#include "graphics.hpp"
#include "state.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace splonks::tools::throw_bomb {

namespace {

bool UseThrowBombTool(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    std::size_t tool_slot_index,
    bool trigger_pressed,
    ToolThrowVelocityBuilder build_throw_velocity,
    std::optional<Vec2> throw_velocity_override
) {
    const VID user_vid = state.ents.ents[ent_idx].vid;
    ToolSlot* const tool_slot = state.ent_tools.FindToolSlotMut(user_vid, tool_slot_index);
    if (tool_slot == nullptr) {
        return false;
    }

    return ents::common::TrySpawnAndThrowEntForToolUse(
        ent_idx,
        state,
        graphics,
        audio,
        *tool_slot,
        trigger_pressed,
        kThrowBombToolSpec.use_cooldown_frames,
        ents::common::kThrownByImmunityDuration,
        [](Ent& spawned_ent) { SetEntAs(spawned_ent, EntType::Bomb); },
        build_throw_velocity,
        throw_velocity_override
    );
}

bool UseThrowStickyBombTool(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    std::size_t tool_slot_index,
    bool trigger_pressed,
    ToolThrowVelocityBuilder build_throw_velocity,
    std::optional<Vec2> throw_velocity_override
) {
    const VID user_vid = state.ents.ents[ent_idx].vid;
    ToolSlot* const tool_slot = state.ent_tools.FindToolSlotMut(user_vid, tool_slot_index);
    if (tool_slot == nullptr) {
        return false;
    }

    return ents::common::TrySpawnAndThrowEntForToolUse(
        ent_idx,
        state,
        graphics,
        audio,
        *tool_slot,
        trigger_pressed,
        kThrowStickyBombToolSpec.use_cooldown_frames,
        ents::common::kThrownByImmunityDuration,
        [](Ent& spawned_ent) {
            SetEntAs(spawned_ent, EntType::Bomb);
            ents::bomb::MarkBombSticky(spawned_ent);
        },
        build_throw_velocity,
        throw_velocity_override
    );
}

} // namespace

extern const ToolSpec kThrowBombToolSpec{
    .kind = ToolKind::ThrowBomb,
    .debug_name = "ThrowBomb",
    .icon_anim_id = aframe_ids::GrenadeUiIcon,
    .use_cooldown_frames = 8,
    .preferred_slot_index = 0,
    .use_fn = UseThrowBombTool,
};

extern const ToolSpec kThrowStickyBombToolSpec{
    .kind = ToolKind::ThrowStickyBomb,
    .debug_name = "ThrowStickyBomb",
    .icon_anim_id = aframe_ids::StickyGrenadeUiIcon,
    .use_cooldown_frames = 8,
    .preferred_slot_index = 0,
    .use_fn = UseThrowStickyBombTool,
};

} // namespace splonks::tools::throw_bomb

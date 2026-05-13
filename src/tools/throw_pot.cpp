#include "tools/throw_pot.hpp"

#include "audio.hpp"
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

namespace splonks::tools::throw_pot {

namespace {

bool UseThrowPotTool(
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
        kThrowPotToolSpec.use_cooldown_frames,
        ents::common::kThrownByImmunityDuration,
        [](Ent& spawned_ent) { SetEntAs(spawned_ent, EntType::Pot); },
        build_throw_velocity,
        throw_velocity_override
    );
}

} // namespace

extern const ToolSpec kThrowPotToolSpec{
    .kind = ToolKind::ThrowPot,
    .debug_name = "ThrowPot",
    .icon_anim_id = aframe_ids::Pot,
    .use_cooldown_frames = 12,
    .use_fn = UseThrowPotTool,
};

} // namespace splonks::tools::throw_pot

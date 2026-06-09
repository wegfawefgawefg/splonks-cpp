#include "tools/throw_rope.hpp"

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

namespace splonks::tools::throw_rope {

namespace {

bool UseThrowRopeTool(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    std::size_t tool_slot_index,
    bool trigger_pressed,
    ToolThrowVelocityBuilder build_throw_velocity,
    std::optional<FxVec2> throw_velocity_override
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
        kThrowRopeToolSpec.use_cooldown_frames,
        ents::common::kThrownByImmunityDuration * 2,
        [](Ent& spawned_ent) { SetEntAs(spawned_ent, EntType::Rope); },
        build_throw_velocity,
        throw_velocity_override
    );
}

} // namespace

extern const ToolSpec kThrowRopeToolSpec{
    .kind = ToolKind::ThrowRope,
    .debug_name = "ThrowRope",
    .icon_anim_id = aframe_ids::RopeUiIcon,
    .use_cooldown_frames = 8,
    .preferred_slot_index = 1,
    .use_fn = UseThrowRopeTool,
};

} // namespace splonks::tools::throw_rope

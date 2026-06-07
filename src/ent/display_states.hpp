#pragma once

#include "ent/display_support.hpp"
#include "aframe_id.hpp"

#include <cstdint>
#include <optional>

namespace splonks {

struct DisplayStateAFrameSelection {
    AFrameId anim_id = kInvalidAFrameId;
    bool animate = true;
    bool has_forced_frame = false;
    std::uint32_t forced_frame = 0;
};

std::optional<DisplayStateAFrameSelection> GetAFrameSelectionForDisplayState(
    const EntDisplayInput& ent
);

} // namespace splonks

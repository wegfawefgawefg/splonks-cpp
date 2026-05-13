#pragma once

#include "entity/display_support.hpp"
#include "frame_data_id.hpp"

#include <cstddef>
#include <optional>

namespace splonks {

struct DisplayStateFrameDataSelection {
    FrameDataId animation_id = kInvalidFrameDataId;
    bool animate = true;
    bool has_forced_frame = false;
    std::size_t forced_frame = 0;
};

std::optional<DisplayStateFrameDataSelection> GetFrameDataSelectionForDisplayState(
    const EntityDisplayInput& entity
);

} // namespace splonks

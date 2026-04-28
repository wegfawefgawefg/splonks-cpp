#include "render/camera.hpp"

#include "stage.hpp"

#include <algorithm>

namespace splonks {

Vec2 ClampCameraTargetToStage(const Stage& stage, Vec2 target) {
    if (!stage.camera_clamp_enabled) {
        return target;
    }

    const Vec2 stage_dims = ToVec2(stage.GetStageDims());
    const Vec2 margin = stage.camera_clamp_margin;
    const Vec2 map_tl_bound = margin;
    const Vec2 map_br_bound = stage_dims - margin;

    if (stage_dims.x <= margin.x * 2.0F) {
        target.x = stage_dims.x / 2.0F;
    } else {
        target.x = std::clamp(target.x, map_tl_bound.x, map_br_bound.x);
    }

    if (stage_dims.y <= margin.y * 2.0F) {
        target.y = stage_dims.y / 2.0F;
    } else {
        target.y = std::clamp(target.y, map_tl_bound.y, map_br_bound.y);
    }

    return target;
}

} // namespace splonks

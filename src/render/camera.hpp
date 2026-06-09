#pragma once

#include "math_types.hpp"

namespace splonks {

struct Stage;

FVec2 ClampCameraTargetToStage(const Stage& stage, FVec2 target);

} // namespace splonks

#pragma once

#include "math_types.hpp"

namespace splonks {

struct Stage;

Vec2 ClampCameraTargetToStage(const Stage& stage, Vec2 target);

} // namespace splonks

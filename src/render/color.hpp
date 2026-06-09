#pragma once

#include "math_types.hpp"

#include <algorithm>
#include <cmath>

namespace splonks {

inline Color3 ClampRenderColor(Color3 color, float min_value = 0.0F, float max_value = 2.0F) {
    color.r = std::clamp(color.r, min_value, max_value);
    color.g = std::clamp(color.g, min_value, max_value);
    color.b = std::clamp(color.b, min_value, max_value);
    return color;
}

inline Color3 MaxRenderColor(Color3 left, Color3 right) {
    return Color3::New(
        std::max(left.r, right.r),
        std::max(left.g, right.g),
        std::max(left.b, right.b)
    );
}

inline Color3 LerpRenderColor(Color3 left, Color3 right, float amount) {
    return Color3::New(
        std::lerp(left.r, right.r, amount),
        std::lerp(left.g, right.g, amount),
        std::lerp(left.b, right.b, amount)
    );
}

} // namespace splonks

#pragma once

#include "math_types.hpp"

#include <gfxp/gfxp.hpp>

#include <cstdint>
#include <limits>

namespace splonks {

using FxScalar = gfxp::Fixed12;
using FxVec2 = gfxp::Vec2_12;
using FxAABB = gfxp::Aabb_12;

struct FxColor3 {
    FxScalar r = FxScalar::from_int(1);
    FxScalar g = FxScalar::from_int(1);
    FxScalar b = FxScalar::from_int(1);
};

[[nodiscard]] constexpr FxVec2 PixelVec2(std::int32_t x, std::int32_t y) {
    return FxVec2::from_pixels(x, y);
}

[[nodiscard]] inline IVec2 ToPixelIVec2Round(FxVec2 value) {
    return IVec2::New(value.x.round_int(), value.y.round_int());
}

[[nodiscard]] inline IVec2 ToPixelIVec2Trunc(FxVec2 value) {
    return IVec2::New(value.x.trunc_int(), value.y.trunc_int());
}

namespace detail {

[[nodiscard]] inline std::uint64_t SqrtFloor(std::uint64_t value) {
    std::uint64_t result = 0;
    std::uint64_t bit = std::uint64_t{1} << 62U;
    while (bit > value) {
        bit >>= 2U;
    }
    while (bit != 0U) {
        if (value >= result + bit) {
            value -= result + bit;
            result = (result >> 1U) + bit;
        } else {
            result >>= 1U;
        }
        bit >>= 2U;
    }
    return result;
}

} // namespace detail

[[nodiscard]] inline FxScalar FxLength(FxVec2 value) {
    const std::int64_t x = value.x.raw_value();
    const std::int64_t y = value.y.raw_value();
    const std::uint64_t sum = static_cast<std::uint64_t>(x * x + y * y);
    const std::uint64_t root = detail::SqrtFloor(sum);
    if (root > static_cast<std::uint64_t>(std::numeric_limits<FxScalar::raw_type>::max())) {
        return FxScalar::from_raw(std::numeric_limits<FxScalar::raw_type>::max());
    }
    return FxScalar::from_raw(static_cast<FxScalar::raw_type>(root));
}

[[nodiscard]] inline FxVec2 FxNormalizeOrZero(FxVec2 value) {
    const FxScalar length = FxLength(value);
    if (length == FxScalar::zero()) {
        return FxVec2::zero();
    }
    return value / length;
}

[[nodiscard]] inline FxScalar ToFxScalar(float value,
                                         gfxp::Rounding rounding = gfxp::Rounding::Nearest) {
    return FxScalar::from_float_for_boundary(value, rounding);
}

[[nodiscard]] inline float ToFloat(FxScalar value) {
    return value.to_float();
}

[[nodiscard]] inline FxVec2 ToFxVec2(const FVec2& value,
                                     gfxp::Rounding rounding = gfxp::Rounding::Nearest) {
    return FxVec2{ToFxScalar(value.x, rounding), ToFxScalar(value.y, rounding)};
}

[[nodiscard]] inline FxVec2 ToFxVec2(float x, float y,
                                     gfxp::Rounding rounding = gfxp::Rounding::Nearest) {
    return FxVec2{ToFxScalar(x, rounding), ToFxScalar(y, rounding)};
}

[[nodiscard]] inline FVec2 ToFVec2(const FxVec2& value) {
    return FVec2::New(value.x.to_float(), value.y.to_float());
}

[[nodiscard]] inline FxColor3 ToFxColor3(const Color3& value,
                                         gfxp::Rounding rounding = gfxp::Rounding::Nearest) {
    return FxColor3{ToFxScalar(value.r, rounding),
                    ToFxScalar(value.g, rounding),
                    ToFxScalar(value.b, rounding)};
}

[[nodiscard]] inline Color3 ToFColor3(const FxColor3& value) {
    return Color3::New(value.r.to_float(), value.g.to_float(), value.b.to_float());
}

} // namespace splonks

#pragma once

#include "math_types.hpp"

#include <gfxp/gfxp.hpp>

#include <cstdint>
#include <limits>

namespace splonks::sim {

using Scalar = gfxp::Fixed12;
using Vec2 = gfxp::Vec2_12;
using AABB = gfxp::Aabb_12;

struct Color3 {
    Scalar r = Scalar::from_int(1);
    Scalar g = Scalar::from_int(1);
    Scalar b = Scalar::from_int(1);
};

[[nodiscard]] inline Scalar ToSimScalar(float value,
                                        gfxp::Rounding rounding = gfxp::Rounding::Nearest) {
    return Scalar::from_float_for_boundary(value, rounding);
}

[[nodiscard]] inline float ToRenderScalar(Scalar value) {
    return value.to_float();
}

[[nodiscard]] inline Vec2 ToSimVec2(const splonks::Vec2& value,
                                    gfxp::Rounding rounding = gfxp::Rounding::Nearest) {
    return Vec2{ToSimScalar(value.x, rounding), ToSimScalar(value.y, rounding)};
}

[[nodiscard]] inline Vec2 ToSimVec2(float x, float y,
                                    gfxp::Rounding rounding = gfxp::Rounding::Nearest) {
    return Vec2{ToSimScalar(x, rounding), ToSimScalar(y, rounding)};
}

[[nodiscard]] inline splonks::Vec2 ToRenderVec2(const Vec2& value) {
    return splonks::Vec2::New(value.x.to_float(), value.y.to_float());
}

[[nodiscard]] constexpr Vec2 PixelVec2(std::int32_t x, std::int32_t y) {
    return Vec2::from_pixels(x, y);
}

[[nodiscard]] inline IVec2 ToPixelIVec2Round(Vec2 value) {
    return IVec2::New(value.x.to_pixels_round(), value.y.to_pixels_round());
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

[[nodiscard]] inline Scalar Length(Vec2 value) {
    const std::int64_t x = value.x.raw_value();
    const std::int64_t y = value.y.raw_value();
    const std::uint64_t sum = static_cast<std::uint64_t>(x * x + y * y);
    const std::uint64_t root = detail::SqrtFloor(sum);
    if (root > static_cast<std::uint64_t>(std::numeric_limits<Scalar::raw_type>::max())) {
        return Scalar::from_raw(std::numeric_limits<Scalar::raw_type>::max());
    }
    return Scalar::from_raw(static_cast<Scalar::raw_type>(root));
}

[[nodiscard]] inline Vec2 NormalizeOrZero(Vec2 value) {
    const Scalar length = Length(value);
    if (length == Scalar::zero()) {
        return Vec2::zero();
    }
    return value / length;
}

[[nodiscard]] inline Color3 ToSimColor3(const splonks::Color3& value,
                                        gfxp::Rounding rounding = gfxp::Rounding::Nearest) {
    return Color3{ToSimScalar(value.r, rounding),
                  ToSimScalar(value.g, rounding),
                  ToSimScalar(value.b, rounding)};
}

[[nodiscard]] inline splonks::Color3 ToRenderColor3(const Color3& value) {
    return splonks::Color3::New(value.r.to_float(), value.g.to_float(), value.b.to_float());
}

} // namespace splonks::sim

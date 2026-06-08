#pragma once

#include "math_types.hpp"

#include <gfxp/gfxp.hpp>

namespace splonks::sim {

using Scalar = gfxp::Fixed12;
using Vec2 = gfxp::BasicVec2<Scalar>;
using AABB = gfxp::BasicAabb<Scalar>;

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

[[nodiscard]] inline splonks::Vec2 ToRenderVec2(const Vec2& value) {
    return splonks::Vec2::New(value.x.to_float(), value.y.to_float());
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

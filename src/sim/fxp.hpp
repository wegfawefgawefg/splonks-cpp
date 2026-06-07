#pragma once

#include "math_types.hpp"

#include <gfxp/gfxp.hpp>

namespace splonks::sim {

using Scalar = gfxp::Fixed12;
using Vec2 = gfxp::BasicVec2<Scalar>;

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

} // namespace splonks::sim

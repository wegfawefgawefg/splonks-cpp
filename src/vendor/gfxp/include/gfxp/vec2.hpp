#pragma once

#include "gfxp/fixed.hpp"

namespace gfxp {

template <typename FixedT> struct BasicVec2 {
    using scalar_type = FixedT;

    FixedT x = FixedT::zero();
    FixedT y = FixedT::zero();

    [[nodiscard]] static constexpr BasicVec2 zero() {
        return BasicVec2{};
    }

    [[nodiscard]] static constexpr BasicVec2 from_raw(typename FixedT::raw_type x_raw,
                                                      typename FixedT::raw_type y_raw) {
        return BasicVec2{FixedT::from_raw(x_raw), FixedT::from_raw(y_raw)};
    }

    [[nodiscard]] static constexpr BasicVec2 from_int(int32_t x, int32_t y) {
        return BasicVec2{FixedT::from_int(x), FixedT::from_int(y)};
    }

    constexpr BasicVec2& operator+=(BasicVec2 other) {
        x += other.x;
        y += other.y;
        return *this;
    }

    constexpr BasicVec2& operator-=(BasicVec2 other) {
        x -= other.x;
        y -= other.y;
        return *this;
    }

    constexpr BasicVec2& operator*=(FixedT scalar) {
        x *= scalar;
        y *= scalar;
        return *this;
    }

    constexpr BasicVec2& operator/=(FixedT scalar) {
        x /= scalar;
        y /= scalar;
        return *this;
    }

    [[nodiscard]] constexpr BasicVec2 operator-() const {
        return BasicVec2{-x, -y};
    }
};

using Vec2 = BasicVec2<Fixed>;
using Vec2_8 = BasicVec2<Fixed8>;
using Vec2_10 = BasicVec2<Fixed10>;
using Vec2_12 = BasicVec2<Fixed12>;
using Vec2_16 = BasicVec2<Fixed16>;

template <typename FixedT>
[[nodiscard]] constexpr BasicVec2<FixedT> operator+(BasicVec2<FixedT> lhs, BasicVec2<FixedT> rhs) {
    lhs += rhs;
    return lhs;
}

template <typename FixedT>
[[nodiscard]] constexpr BasicVec2<FixedT> operator-(BasicVec2<FixedT> lhs, BasicVec2<FixedT> rhs) {
    lhs -= rhs;
    return lhs;
}

template <typename FixedT>
[[nodiscard]] constexpr BasicVec2<FixedT> operator*(BasicVec2<FixedT> lhs, FixedT scalar) {
    lhs *= scalar;
    return lhs;
}

template <typename FixedT>
[[nodiscard]] constexpr BasicVec2<FixedT> operator*(FixedT scalar, BasicVec2<FixedT> rhs) {
    return rhs * scalar;
}

template <typename FixedT>
[[nodiscard]] constexpr BasicVec2<FixedT> operator/(BasicVec2<FixedT> lhs, FixedT scalar) {
    lhs /= scalar;
    return lhs;
}

template <typename FixedT>
[[nodiscard]] constexpr bool operator==(BasicVec2<FixedT> lhs, BasicVec2<FixedT> rhs) {
    return lhs.x == rhs.x && lhs.y == rhs.y;
}

template <typename FixedT>
[[nodiscard]] constexpr bool operator!=(BasicVec2<FixedT> lhs, BasicVec2<FixedT> rhs) {
    return !(lhs == rhs);
}

template <typename FixedT>
[[nodiscard]] constexpr FixedT dot(BasicVec2<FixedT> lhs, BasicVec2<FixedT> rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y;
}

template <typename FixedT> [[nodiscard]] constexpr FixedT length_sq(BasicVec2<FixedT> value) {
    return dot(value, value);
}

template <typename FixedT>
[[nodiscard]] constexpr FixedT manhattan_length(BasicVec2<FixedT> value) {
    return value.x.abs() + value.y.abs();
}

template <typename FixedT> [[nodiscard]] constexpr BasicVec2<FixedT> abs(BasicVec2<FixedT> value) {
    return BasicVec2<FixedT>{value.x.abs(), value.y.abs()};
}

template <typename FixedT>
[[nodiscard]] constexpr BasicVec2<FixedT> min(BasicVec2<FixedT> lhs, BasicVec2<FixedT> rhs) {
    return BasicVec2<FixedT>{min(lhs.x, rhs.x), min(lhs.y, rhs.y)};
}

template <typename FixedT>
[[nodiscard]] constexpr BasicVec2<FixedT> max(BasicVec2<FixedT> lhs, BasicVec2<FixedT> rhs) {
    return BasicVec2<FixedT>{max(lhs.x, rhs.x), max(lhs.y, rhs.y)};
}

template <typename FixedT>
[[nodiscard]] constexpr BasicVec2<FixedT> clamp(BasicVec2<FixedT> value, BasicVec2<FixedT> low,
                                                BasicVec2<FixedT> high) {
    return min(max(value, low), high);
}

template <typename ToVec2, typename FixedT>
[[nodiscard]] constexpr std::optional<ToVec2>
checked_vec2_cast(BasicVec2<FixedT> value, Rounding rounding = Rounding::Nearest) {
    using ToFixed = typename ToVec2::scalar_type;
    const std::optional<ToFixed> x = checked_fixed_cast<ToFixed>(value.x, rounding);
    const std::optional<ToFixed> y = checked_fixed_cast<ToFixed>(value.y, rounding);
    if (!x || !y)
        return std::nullopt;
    return ToVec2{*x, *y};
}

template <typename ToVec2, typename FixedT>
[[nodiscard]] constexpr ToVec2 vec2_cast(BasicVec2<FixedT> value,
                                         Rounding rounding = Rounding::Nearest) {
    const std::optional<ToVec2> converted = checked_vec2_cast<ToVec2>(value, rounding);
    return converted.value_or(ToVec2::zero());
}

} // namespace gfxp

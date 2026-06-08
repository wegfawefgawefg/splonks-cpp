#pragma once

#include "gfxp/vec2.hpp"

namespace gfxp {

template <typename FixedT> struct BasicAabb {
    using scalar_type = FixedT;
    using vec2_type = BasicVec2<FixedT>;

    vec2_type tl = vec2_type::zero();
    vec2_type br = vec2_type::zero();

    [[nodiscard]] static constexpr BasicAabb zero() {
        return BasicAabb{};
    }

    [[nodiscard]] static constexpr BasicAabb from_corners(vec2_type top_left,
                                                          vec2_type bottom_right) {
        return BasicAabb{top_left, bottom_right};
    }

    [[nodiscard]] static constexpr BasicAabb from_pos_size(vec2_type pos, vec2_type size) {
        return BasicAabb{pos, pos + size};
    }

    [[nodiscard]] constexpr vec2_type size() const {
        return br - tl;
    }

    [[nodiscard]] constexpr vec2_type center() const {
        return (tl + br) / FixedT::from_int(2);
    }

    constexpr BasicAabb& translate(vec2_type delta) {
        tl += delta;
        br += delta;
        return *this;
    }
};

using Aabb = BasicAabb<Fixed>;
using Aabb_8 = BasicAabb<Fixed8>;
using Aabb_10 = BasicAabb<Fixed10>;
using Aabb_12 = BasicAabb<Fixed12>;
using Aabb_16 = BasicAabb<Fixed16>;

template <typename FixedT>
[[nodiscard]] constexpr bool operator==(BasicAabb<FixedT> lhs, BasicAabb<FixedT> rhs) {
    return lhs.tl == rhs.tl && lhs.br == rhs.br;
}

template <typename FixedT>
[[nodiscard]] constexpr bool operator!=(BasicAabb<FixedT> lhs, BasicAabb<FixedT> rhs) {
    return !(lhs == rhs);
}

template <typename FixedT>
[[nodiscard]] constexpr BasicAabb<FixedT> translate(BasicAabb<FixedT> aabb,
                                                    BasicVec2<FixedT> delta) {
    aabb.translate(delta);
    return aabb;
}

template <typename FixedT>
[[nodiscard]] constexpr bool aabbs_intersect(BasicAabb<FixedT> left, BasicAabb<FixedT> right) {
    if (left.br.x < right.tl.x)
        return false;
    if (left.tl.x > right.br.x)
        return false;
    if (left.br.y < right.tl.y)
        return false;
    if (left.tl.y > right.br.y)
        return false;
    return true;
}

template <typename FixedT>
[[nodiscard]] constexpr BasicVec2<FixedT> min_displacement(BasicAabb<FixedT> aabb1,
                                                           BasicAabb<FixedT> aabb2) {
    FixedT dx = FixedT::zero();
    if (aabb1.br.x < aabb2.tl.x) {
        dx = aabb2.tl.x - aabb1.br.x;
    } else if (aabb1.tl.x > aabb2.br.x) {
        dx = aabb2.br.x - aabb1.tl.x;
    }

    FixedT dy = FixedT::zero();
    if (aabb1.br.y < aabb2.tl.y) {
        dy = aabb2.tl.y - aabb1.br.y;
    } else if (aabb1.tl.y > aabb2.br.y) {
        dy = aabb2.br.y - aabb1.tl.y;
    }

    return BasicVec2<FixedT>{dx, dy};
}

template <typename ToAabb, typename FixedT>
[[nodiscard]] constexpr std::optional<ToAabb>
checked_aabb_cast(BasicAabb<FixedT> value, Rounding rounding = Rounding::Nearest) {
    using ToVec2 = typename ToAabb::vec2_type;
    const std::optional<ToVec2> tl = checked_vec2_cast<ToVec2>(value.tl, rounding);
    const std::optional<ToVec2> br = checked_vec2_cast<ToVec2>(value.br, rounding);
    if (!tl || !br)
        return std::nullopt;
    return ToAabb{*tl, *br};
}

template <typename ToAabb, typename FixedT>
[[nodiscard]] constexpr ToAabb aabb_cast(BasicAabb<FixedT> value,
                                         Rounding rounding = Rounding::Nearest) {
    const std::optional<ToAabb> converted = checked_aabb_cast<ToAabb>(value, rounding);
    return converted.value_or(ToAabb::zero());
}

} // namespace gfxp

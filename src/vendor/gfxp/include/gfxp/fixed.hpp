#pragma once

#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string_view>
#include <type_traits>

namespace gfxp {

enum class Rounding {
    TowardZero,
    Floor,
    Ceil,
    Nearest,
};

namespace detail {

constexpr bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

constexpr std::optional<int64_t> div_round(int64_t numerator, int64_t denominator,
                                           Rounding rounding) {
    if (denominator == 0)
        return std::nullopt;

    if (denominator < 0) {
        numerator = -numerator;
        denominator = -denominator;
    }

    const int64_t quotient = numerator / denominator;
    const int64_t remainder = numerator % denominator;
    if (remainder == 0)
        return quotient;

    switch (rounding) {
    case Rounding::TowardZero:
        return quotient;
    case Rounding::Floor:
        return numerator < 0 ? quotient - 1 : quotient;
    case Rounding::Ceil:
        return numerator > 0 ? quotient + 1 : quotient;
    case Rounding::Nearest: {
        const int64_t abs_remainder = remainder < 0 ? -remainder : remainder;
        const bool round_up = abs_remainder * 2 >= denominator;
        if (!round_up)
            return quotient;
        return numerator > 0 ? quotient + 1 : quotient - 1;
    }
    }

    return std::nullopt;
}

template <typename Raw> constexpr std::optional<Raw> checked_raw(int64_t value) {
    static_assert(std::is_integral_v<Raw>);
    static_assert(std::is_signed_v<Raw>);

    if (value < static_cast<int64_t>(std::numeric_limits<Raw>::min()) ||
        value > static_cast<int64_t>(std::numeric_limits<Raw>::max())) {
        return std::nullopt;
    }
    return static_cast<Raw>(value);
}

} // namespace detail

template <typename Raw, int FracBits> struct BasicFixed {
    static_assert(std::is_integral_v<Raw>);
    static_assert(std::is_signed_v<Raw>);
    static_assert(FracBits > 0);
    static_assert(FracBits < static_cast<int>(sizeof(Raw) * 8 - 1));

    using raw_type = Raw;
    using wide_type = int64_t;

    Raw raw = 0;

    static constexpr int frac_bits = FracBits;
    static constexpr Raw scale = static_cast<Raw>(Raw{1} << frac_bits);

    [[nodiscard]] static constexpr BasicFixed zero() {
        return BasicFixed{};
    }

    [[nodiscard]] static constexpr BasicFixed from_raw(Raw raw_value) {
        return BasicFixed{raw_value};
    }

    [[nodiscard]] static constexpr BasicFixed from_int(int32_t value) {
        return BasicFixed{static_cast<Raw>(value * static_cast<int32_t>(scale))};
    }

    [[nodiscard]] static constexpr BasicFixed from_pixels(int32_t value) {
        return from_int(value);
    }

    [[nodiscard]] static constexpr std::optional<BasicFixed> checked_from_int(int32_t value) {
        const wide_type raw_value = static_cast<wide_type>(value) * static_cast<wide_type>(scale);
        const std::optional<Raw> checked = detail::checked_raw<Raw>(raw_value);
        if (!checked)
            return std::nullopt;
        return BasicFixed{*checked};
    }

    [[nodiscard]] static constexpr std::optional<BasicFixed> checked_from_pixels(int32_t value) {
        return checked_from_int(value);
    }

    [[nodiscard]] static constexpr std::optional<BasicFixed>
    from_ratio(int64_t numerator, int64_t denominator, Rounding rounding = Rounding::Nearest) {
        const std::optional<int64_t> rounded =
            detail::div_round(numerator * static_cast<int64_t>(scale), denominator, rounding);
        if (!rounded)
            return std::nullopt;

        const std::optional<Raw> checked = detail::checked_raw<Raw>(*rounded);
        if (!checked)
            return std::nullopt;
        return BasicFixed{*checked};
    }

    [[nodiscard]] static constexpr std::optional<BasicFixed>
    from_decimal(std::string_view text, Rounding rounding = Rounding::Nearest) {
        if (text.empty())
            return std::nullopt;

        bool negative = false;
        std::size_t index = 0;
        if (text[index] == '+' || text[index] == '-') {
            negative = text[index] == '-';
            ++index;
        }

        bool saw_digit = false;
        int64_t whole = 0;
        while (index < text.size() && detail::is_digit(text[index])) {
            saw_digit = true;
            const int digit = text[index] - '0';
            if (whole > (std::numeric_limits<int64_t>::max() - digit) / 10)
                return std::nullopt;
            whole = whole * 10 + digit;
            ++index;
        }

        int64_t fractional = 0;
        int64_t fractional_denominator = 1;
        if (index < text.size() && text[index] == '.') {
            ++index;
            while (index < text.size() && detail::is_digit(text[index])) {
                saw_digit = true;
                const int digit = text[index] - '0';
                if (fractional_denominator > std::numeric_limits<int64_t>::max() / 10)
                    return std::nullopt;
                fractional_denominator *= 10;
                if (fractional > (std::numeric_limits<int64_t>::max() - digit) / 10)
                    return std::nullopt;
                fractional = fractional * 10 + digit;
                ++index;
            }
        }

        if (!saw_digit || index != text.size())
            return std::nullopt;

        if (whole > std::numeric_limits<int64_t>::max() / fractional_denominator)
            return std::nullopt;
        int64_t numerator = whole * fractional_denominator + fractional;
        if (negative)
            numerator = -numerator;

        return from_ratio(numerator, fractional_denominator, rounding);
    }

    [[nodiscard]] static BasicFixed from_float_for_boundary(float value,
                                                            Rounding rounding = Rounding::Nearest) {
        const float scaled = value * static_cast<float>(scale);
        switch (rounding) {
        case Rounding::TowardZero:
            return BasicFixed{static_cast<Raw>(scaled)};
        case Rounding::Floor:
            return BasicFixed{static_cast<Raw>(std::floor(scaled))};
        case Rounding::Ceil:
            return BasicFixed{static_cast<Raw>(std::ceil(scaled))};
        case Rounding::Nearest:
            return BasicFixed{static_cast<Raw>(std::round(scaled))};
        }
        return BasicFixed{};
    }

    [[nodiscard]] constexpr Raw raw_value() const {
        return raw;
    }

    [[nodiscard]] constexpr float to_float() const {
        return static_cast<float>(raw) / static_cast<float>(scale);
    }

    [[nodiscard]] constexpr double to_double() const {
        return static_cast<double>(raw) / static_cast<double>(scale);
    }

    [[nodiscard]] constexpr int32_t trunc_int() const {
        return static_cast<int32_t>(raw / scale);
    }

    [[nodiscard]] constexpr int32_t floor_int() const {
        const Raw quotient = static_cast<Raw>(raw / scale);
        const Raw remainder = static_cast<Raw>(raw % scale);
        if (remainder != 0 && raw < 0)
            return static_cast<int32_t>(quotient - 1);
        return static_cast<int32_t>(quotient);
    }

    [[nodiscard]] constexpr int32_t to_pixels_floor() const {
        return floor_int();
    }

    [[nodiscard]] constexpr int32_t ceil_int() const {
        const Raw quotient = static_cast<Raw>(raw / scale);
        const Raw remainder = static_cast<Raw>(raw % scale);
        if (remainder != 0 && raw > 0)
            return static_cast<int32_t>(quotient + 1);
        return static_cast<int32_t>(quotient);
    }

    [[nodiscard]] constexpr int32_t to_pixels_ceil() const {
        return ceil_int();
    }

    [[nodiscard]] constexpr int32_t round_int() const {
        const std::optional<int64_t> rounded = detail::div_round(raw, scale, Rounding::Nearest);
        return rounded ? static_cast<int32_t>(*rounded) : 0;
    }

    [[nodiscard]] constexpr int32_t to_pixels_round() const {
        return round_int();
    }

    [[nodiscard]] constexpr int32_t to_pixels_trunc() const {
        return trunc_int();
    }

    [[nodiscard]] constexpr BasicFixed abs() const {
        return raw < 0 ? BasicFixed{static_cast<Raw>(-raw)} : *this;
    }

    [[nodiscard]] constexpr int sign() const {
        if (raw < 0)
            return -1;
        if (raw > 0)
            return 1;
        return 0;
    }

    constexpr BasicFixed& operator+=(BasicFixed other) {
        raw = static_cast<Raw>(raw + other.raw);
        return *this;
    }

    constexpr BasicFixed& operator-=(BasicFixed other) {
        raw = static_cast<Raw>(raw - other.raw);
        return *this;
    }

    constexpr BasicFixed& operator*=(BasicFixed other) {
        raw = static_cast<Raw>((static_cast<wide_type>(raw) * other.raw) >> frac_bits);
        return *this;
    }

    constexpr BasicFixed& operator/=(BasicFixed other) {
        const std::optional<int64_t> divided = detail::div_round(
            static_cast<wide_type>(raw) << frac_bits, other.raw, Rounding::TowardZero);
        raw = divided ? static_cast<Raw>(*divided) : 0;
        return *this;
    }

    [[nodiscard]] constexpr BasicFixed operator-() const {
        return BasicFixed{static_cast<Raw>(-raw)};
    }
};

using Fixed8 = BasicFixed<int32_t, 8>;
using Fixed10 = BasicFixed<int32_t, 10>;
using Fixed12 = BasicFixed<int32_t, 12>;
using Fixed16 = BasicFixed<int32_t, 16>;
using Fixed = Fixed12;

template <typename ToFixed, typename FromRaw, int FromFracBits>
[[nodiscard]] constexpr std::optional<ToFixed>
checked_fixed_cast(BasicFixed<FromRaw, FromFracBits> value, Rounding rounding = Rounding::Nearest) {
    const int shift = ToFixed::frac_bits - FromFracBits;
    int64_t raw = value.raw_value();
    if (shift > 0) {
        raw <<= shift;
    } else if (shift < 0) {
        const int64_t denominator = int64_t{1} << -shift;
        const std::optional<int64_t> rounded = detail::div_round(raw, denominator, rounding);
        if (!rounded)
            return std::nullopt;
        raw = *rounded;
    }

    const std::optional<typename ToFixed::raw_type> checked =
        detail::checked_raw<typename ToFixed::raw_type>(raw);
    if (!checked)
        return std::nullopt;
    return ToFixed::from_raw(*checked);
}

template <typename ToFixed, typename FromRaw, int FromFracBits>
[[nodiscard]] constexpr ToFixed fixed_cast(BasicFixed<FromRaw, FromFracBits> value,
                                           Rounding rounding = Rounding::Nearest) {
    const std::optional<ToFixed> converted = checked_fixed_cast<ToFixed>(value, rounding);
    return converted.value_or(ToFixed::zero());
}

template <typename Raw, int FracBits>
[[nodiscard]] constexpr BasicFixed<Raw, FracBits> operator+(BasicFixed<Raw, FracBits> lhs,
                                                            BasicFixed<Raw, FracBits> rhs) {
    lhs += rhs;
    return lhs;
}

template <typename Raw, int FracBits>
[[nodiscard]] constexpr BasicFixed<Raw, FracBits> operator-(BasicFixed<Raw, FracBits> lhs,
                                                            BasicFixed<Raw, FracBits> rhs) {
    lhs -= rhs;
    return lhs;
}

template <typename Raw, int FracBits>
[[nodiscard]] constexpr BasicFixed<Raw, FracBits> operator*(BasicFixed<Raw, FracBits> lhs,
                                                            BasicFixed<Raw, FracBits> rhs) {
    lhs *= rhs;
    return lhs;
}

template <typename Raw, int FracBits>
[[nodiscard]] constexpr BasicFixed<Raw, FracBits> operator/(BasicFixed<Raw, FracBits> lhs,
                                                            BasicFixed<Raw, FracBits> rhs) {
    lhs /= rhs;
    return lhs;
}

template <typename Raw, int FracBits>
[[nodiscard]] constexpr BasicFixed<Raw, FracBits> operator*(BasicFixed<Raw, FracBits> lhs,
                                                            int32_t rhs) {
    return BasicFixed<Raw, FracBits>{static_cast<Raw>(lhs.raw * rhs)};
}

template <typename Raw, int FracBits>
[[nodiscard]] constexpr BasicFixed<Raw, FracBits> operator*(int32_t lhs,
                                                            BasicFixed<Raw, FracBits> rhs) {
    return rhs * lhs;
}

template <typename Raw, int FracBits>
[[nodiscard]] constexpr BasicFixed<Raw, FracBits> operator/(BasicFixed<Raw, FracBits> lhs,
                                                            int32_t rhs) {
    return BasicFixed<Raw, FracBits>{static_cast<Raw>(lhs.raw / rhs)};
}

template <typename Raw, int FracBits>
[[nodiscard]] constexpr bool operator==(BasicFixed<Raw, FracBits> lhs,
                                        BasicFixed<Raw, FracBits> rhs) {
    return lhs.raw == rhs.raw;
}

template <typename Raw, int FracBits>
[[nodiscard]] constexpr bool operator!=(BasicFixed<Raw, FracBits> lhs,
                                        BasicFixed<Raw, FracBits> rhs) {
    return !(lhs == rhs);
}

template <typename Raw, int FracBits>
[[nodiscard]] constexpr bool operator<(BasicFixed<Raw, FracBits> lhs,
                                       BasicFixed<Raw, FracBits> rhs) {
    return lhs.raw < rhs.raw;
}

template <typename Raw, int FracBits>
[[nodiscard]] constexpr bool operator<=(BasicFixed<Raw, FracBits> lhs,
                                        BasicFixed<Raw, FracBits> rhs) {
    return lhs.raw <= rhs.raw;
}

template <typename Raw, int FracBits>
[[nodiscard]] constexpr bool operator>(BasicFixed<Raw, FracBits> lhs,
                                       BasicFixed<Raw, FracBits> rhs) {
    return lhs.raw > rhs.raw;
}

template <typename Raw, int FracBits>
[[nodiscard]] constexpr bool operator>=(BasicFixed<Raw, FracBits> lhs,
                                        BasicFixed<Raw, FracBits> rhs) {
    return lhs.raw >= rhs.raw;
}

template <typename Raw, int FracBits>
[[nodiscard]] constexpr BasicFixed<Raw, FracBits> min(BasicFixed<Raw, FracBits> lhs,
                                                      BasicFixed<Raw, FracBits> rhs) {
    return lhs < rhs ? lhs : rhs;
}

template <typename Raw, int FracBits>
[[nodiscard]] constexpr BasicFixed<Raw, FracBits> max(BasicFixed<Raw, FracBits> lhs,
                                                      BasicFixed<Raw, FracBits> rhs) {
    return lhs > rhs ? lhs : rhs;
}

template <typename Raw, int FracBits>
[[nodiscard]] constexpr BasicFixed<Raw, FracBits> clamp(BasicFixed<Raw, FracBits> value,
                                                        BasicFixed<Raw, FracBits> low,
                                                        BasicFixed<Raw, FracBits> high) {
    return min(max(value, low), high);
}

} // namespace gfxp

namespace std {

template <typename Raw, int FracBits> struct hash<gfxp::BasicFixed<Raw, FracBits>> {
    std::size_t operator()(gfxp::BasicFixed<Raw, FracBits> value) const noexcept {
        return std::hash<Raw>{}(value.raw_value());
    }
};

} // namespace std

#pragma once

#include <cstdint>
#include <cmath>

namespace splonks {

struct Vec2 {
    float x = 0.0F;
    float y = 0.0F;

    static Vec2 New(float x_value, float y_value) {
        Vec2 result;
        result.x = x_value;
        result.y = y_value;
        return result;
    }
};

struct IVec2 {
    int x = 0;
    int y = 0;

    static IVec2 New(int x_value, int y_value) {
        IVec2 result;
        result.x = x_value;
        result.y = y_value;
        return result;
    }
};

struct UVec2 {
    std::uint32_t x = 0;
    std::uint32_t y = 0;

    static UVec2 New(std::uint32_t x_value, std::uint32_t y_value) {
        UVec2 result;
        result.x = x_value;
        result.y = y_value;
        return result;
    }
};

struct Color3 {
    float r = 1.0F;
    float g = 1.0F;
    float b = 1.0F;

    static constexpr Color3 New(float r_value, float g_value, float b_value) {
        Color3 result;
        result.r = r_value;
        result.g = g_value;
        result.b = b_value;
        return result;
    }

    static constexpr Color3 White(float value = 1.0F) {
        return New(value, value, value);
    }
};

inline Vec2 operator+(const Vec2& left, const Vec2& right) {
    return Vec2::New(left.x + right.x, left.y + right.y);
}

inline IVec2 operator+(const IVec2& left, const IVec2& right) {
    return IVec2::New(left.x + right.x, left.y + right.y);
}

inline UVec2 operator+(const UVec2& left, const UVec2& right) {
    return UVec2::New(left.x + right.x, left.y + right.y);
}

inline Color3 operator+(const Color3& left, const Color3& right) {
    return Color3::New(left.r + right.r, left.g + right.g, left.b + right.b);
}

inline UVec2 operator-(const UVec2& left, const UVec2& right) {
    return UVec2::New(left.x - right.x, left.y - right.y);
}

inline Vec2 operator-(const Vec2& left, const Vec2& right) {
    return Vec2::New(left.x - right.x, left.y - right.y);
}

inline IVec2 operator-(const IVec2& left, const IVec2& right) {
    return IVec2::New(left.x - right.x, left.y - right.y);
}

inline bool operator==(const Vec2& left, const Vec2& right) {
    return left.x == right.x && left.y == right.y;
}

inline bool operator==(const IVec2& left, const IVec2& right) {
    return left.x == right.x && left.y == right.y;
}

inline bool operator==(const UVec2& left, const UVec2& right) {
    return left.x == right.x && left.y == right.y;
}

inline Vec2& operator+=(Vec2& left, const Vec2& right) {
    left.x += right.x;
    left.y += right.y;
    return left;
}

inline Vec2 operator*(const Vec2& left, float right) {
    return Vec2::New(left.x * right, left.y * right);
}

inline Color3 operator*(const Color3& left, float right) {
    return Color3::New(left.r * right, left.g * right, left.b * right);
}

inline IVec2 operator*(const IVec2& left, int right) {
    return IVec2::New(left.x * right, left.y * right);
}

inline UVec2 operator*(const UVec2& left, std::uint32_t right) {
    return UVec2::New(left.x * right, left.y * right);
}

inline UVec2 operator*(const UVec2& left, const UVec2& right) {
    return UVec2::New(left.x * right.x, left.y * right.y);
}

inline Vec2 operator*(float left, const Vec2& right) {
    return Vec2::New(left * right.x, left * right.y);
}

inline Vec2 operator/(const Vec2& left, float right) {
    return Vec2::New(left.x / right, left.y / right);
}

inline Color3 operator/(const Color3& left, float right) {
    return Color3::New(left.r / right, left.g / right, left.b / right);
}

inline IVec2 operator/(const IVec2& left, int right) {
    return IVec2::New(left.x / right, left.y / right);
}

inline UVec2 operator/(const UVec2& left, std::uint32_t right) {
    return UVec2::New(left.x / right, left.y / right);
}

inline IVec2 ToIVec2(const Vec2& value) {
    return IVec2::New(static_cast<int>(value.x), static_cast<int>(value.y));
}

inline int FloorToInt(float value) {
    const int truncated = static_cast<int>(value);
    return static_cast<float>(truncated) > value ? truncated - 1 : truncated;
}

inline int RoundToInt(float value) {
    if (value >= 0.0F) {
        return FloorToInt(value + 0.5F);
    }
    return -FloorToInt((-value) + 0.5F);
}

inline int CeilToInt(float value) {
    const int floored = FloorToInt(value);
    return static_cast<float>(floored) < value ? floored + 1 : floored;
}

inline std::uint64_t IntegerSqrtFloor(std::uint64_t value) {
    std::uint64_t result = 0;
    std::uint64_t bit = std::uint64_t{1} << 62U;
    while (bit > value) {
        bit >>= 2U;
    }
    while (bit != 0) {
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

inline std::int64_t DivRoundNearest(std::int64_t numerator, std::int64_t denominator) {
    if (denominator == 0) {
        return 0;
    }
    const bool negative = numerator < 0;
    const std::uint64_t abs_numerator =
        negative ? static_cast<std::uint64_t>(-numerator) : static_cast<std::uint64_t>(numerator);
    const std::uint64_t abs_denominator =
        denominator < 0 ? static_cast<std::uint64_t>(-denominator) : static_cast<std::uint64_t>(denominator);
    const std::uint64_t rounded = (abs_numerator + (abs_denominator / 2U)) / abs_denominator;
    const std::int64_t signed_result = static_cast<std::int64_t>(rounded);
    return negative == (denominator > 0) ? -signed_result : signed_result;
}

inline IVec2 ToIVec2(const UVec2& value) {
    return IVec2::New(static_cast<int>(value.x), static_cast<int>(value.y));
}

inline UVec2 ToUVec2(const IVec2& value) {
    return UVec2::New(static_cast<std::uint32_t>(value.x), static_cast<std::uint32_t>(value.y));
}

inline Vec2 ToVec2(const UVec2& value) {
    return Vec2::New(static_cast<float>(value.x), static_cast<float>(value.y));
}

inline Vec2 ToVec2(const IVec2& value) {
    return Vec2::New(static_cast<float>(value.x), static_cast<float>(value.y));
}

inline Vec2 Max(const Vec2& value, const Vec2& minimum) {
    return Vec2::New(value.x > minimum.x ? value.x : minimum.x,
                     value.y > minimum.y ? value.y : minimum.y);
}

inline float Min(float left, float right) {
    return left < right ? left : right;
}

inline float Max(float left, float right) {
    return left > right ? left : right;
}

inline float Length(const Vec2& value) {
    return std::sqrt((value.x * value.x) + (value.y * value.y));
}

inline float LengthSquared(const Vec2& value) {
    return (value.x * value.x) + (value.y * value.y);
}

inline Vec2 NormalizeOrZero(const Vec2& value) {
    const float length = Length(value);
    if (length == 0.0F) {
        return Vec2::New(0.0F, 0.0F);
    }
    return value / length;
}

inline float LengthDeterministic(const Vec2& value) {
    constexpr std::int64_t kScale = 4096;
    const std::int64_t x = static_cast<std::int64_t>(RoundToInt(value.x * static_cast<float>(kScale)));
    const std::int64_t y = static_cast<std::int64_t>(RoundToInt(value.y * static_cast<float>(kScale)));
    const std::uint64_t x_sq = static_cast<std::uint64_t>(x * x);
    const std::uint64_t y_sq = static_cast<std::uint64_t>(y * y);
    const std::uint64_t length = IntegerSqrtFloor(x_sq + y_sq);
    return static_cast<float>(length) / static_cast<float>(kScale);
}

inline Vec2 NormalizeOrZeroDeterministic(const Vec2& value) {
    constexpr std::int64_t kScale = 4096;
    const std::int64_t x = static_cast<std::int64_t>(RoundToInt(value.x * static_cast<float>(kScale)));
    const std::int64_t y = static_cast<std::int64_t>(RoundToInt(value.y * static_cast<float>(kScale)));
    const std::uint64_t x_sq = static_cast<std::uint64_t>(x * x);
    const std::uint64_t y_sq = static_cast<std::uint64_t>(y * y);
    const std::int64_t length = static_cast<std::int64_t>(IntegerSqrtFloor(x_sq + y_sq));
    if (length == 0) {
        return Vec2::New(0.0F, 0.0F);
    }
    const std::int64_t normalized_x = DivRoundNearest(x * kScale, length);
    const std::int64_t normalized_y = DivRoundNearest(y * kScale, length);
    return Vec2::New(static_cast<float>(normalized_x) / static_cast<float>(kScale),
                     static_cast<float>(normalized_y) / static_cast<float>(kScale));
}

} // namespace splonks

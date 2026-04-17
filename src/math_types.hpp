#pragma once

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
    unsigned int x = 0;
    unsigned int y = 0;

    static UVec2 New(unsigned int x_value, unsigned int y_value) {
        UVec2 result;
        result.x = x_value;
        result.y = y_value;
        return result;
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

inline IVec2 operator*(const IVec2& left, int right) {
    return IVec2::New(left.x * right, left.y * right);
}

inline UVec2 operator*(const UVec2& left, unsigned int right) {
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

inline IVec2 operator/(const IVec2& left, int right) {
    return IVec2::New(left.x / right, left.y / right);
}

inline UVec2 operator/(const UVec2& left, unsigned int right) {
    return UVec2::New(left.x / right, left.y / right);
}

inline IVec2 ToIVec2(const Vec2& value) {
    return IVec2::New(static_cast<int>(value.x), static_cast<int>(value.y));
}

inline IVec2 ToIVec2(const UVec2& value) {
    return IVec2::New(static_cast<int>(value.x), static_cast<int>(value.y));
}

inline UVec2 ToUVec2(const IVec2& value) {
    return UVec2::New(static_cast<unsigned int>(value.x), static_cast<unsigned int>(value.y));
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

inline Vec2 NormalizeOrZero(const Vec2& value) {
    const float length = Length(value);
    if (length == 0.0F) {
        return Vec2::New(0.0F, 0.0F);
    }
    return value / length;
}

} // namespace splonks

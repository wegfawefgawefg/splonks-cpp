#pragma once

#include "math_types.hpp"

#include <cstdint>

namespace splonks {

struct DeterministicRng {
    std::uint64_t state = 0;

    static DeterministicRng New(std::uint32_t seed);
    std::uint32_t NextU32();
    int RandomIntInclusive(int minimum, int maximum);
    int RandomIntExclusive(int minimum, int maximum);
    float RandomFloat(float minimum, float maximum);
};

struct AABB {
    Vec2 tl;
    Vec2 br;

    static AABB New(const Vec2& top_left, const Vec2& bottom_right);
    struct IAABB AsIAABB() const;
};

struct IAABB {
    IVec2 tl;
    IVec2 br;

    static IAABB New(const IVec2& top_left, const IVec2& bottom_right);
    AABB AsAABB() const;
};

Vec2 GetMinDisplacement(const AABB& aabb1, const AABB& aabb2);
bool AabbsIntersect(const AABB& left, const AABB& right);

namespace rng {

void SetSeed(std::uint32_t seed);
int RandomIntInclusive(int minimum, int maximum);
int RandomIntExclusive(int minimum, int maximum);
float RandomFloat(float minimum, float maximum);

} // namespace rng

} // namespace splonks

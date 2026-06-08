#pragma once

#include "math_types.hpp"
#include "sim/fxp.hpp"

#include <cstdint>

namespace splonks {

struct DetRng {
    std::uint64_t state = 0;

    static DetRng New(std::uint32_t seed);
    std::uint32_t NextU32();
    int RandomIntInclusive(int minimum, int maximum);
    int RandomIntExclusive(int minimum, int maximum);
    float RandomFloat(float minimum, float maximum);
};

struct RenderAABB {
    Vec2 tl;
    Vec2 br;

    static RenderAABB New(const Vec2& top_left, const Vec2& bottom_right);
    struct IAABB AsIAABB() const;
};

struct IAABB {
    IVec2 tl;
    IVec2 br;

    static IAABB New(const IVec2& top_left, const IVec2& bottom_right);
    RenderAABB AsRenderAABB() const;
};

Vec2 GetMinDisplacement(const RenderAABB& aabb1, const RenderAABB& aabb2);
bool AabbsIntersect(const RenderAABB& left, const RenderAABB& right);

sim::AABB ToSimAABB(const RenderAABB& value, gfxp::Rounding rounding = gfxp::Rounding::Nearest);
RenderAABB ToRenderAABB(const sim::AABB& value);
IAABB ToIAABBFloorCeil(const sim::AABB& value);
Vec2 ToRenderMinDisplacement(sim::AABB aabb1, sim::AABB aabb2);

namespace rng {

void SetSeed(std::uint32_t seed);
std::uint32_t RandomU32();
int RandomIntInclusive(int minimum, int maximum);
int RandomIntExclusive(int minimum, int maximum);
float RandomFloat(float minimum, float maximum);

} // namespace rng

} // namespace splonks

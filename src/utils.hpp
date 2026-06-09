#pragma once

#include "math_types.hpp"
#include "fxp.hpp"

#include <cstdint>

namespace splonks {

struct DetRng {
    std::uint64_t state = 0;

    static DetRng New(std::uint32_t seed);
    std::uint32_t NextU32();
    int RandomIntInclusive(int minimum, int maximum);
    int RandomIntExclusive(int minimum, int maximum);
    bool RandomBool();
    float RandomFloat(float minimum, float maximum);
};

FxScalar RandomFxScalar(DetRng& rng, FxScalar minimum, FxScalar maximum);

struct FAABB {
    FVec2 tl;
    FVec2 br;

    static FAABB New(const FVec2& top_left, const FVec2& bottom_right);
    struct IAABB AsIAABB() const;
};

struct IAABB {
    IVec2 tl;
    IVec2 br;

    static IAABB New(const IVec2& top_left, const IVec2& bottom_right);
    FAABB AsFAABB() const;
};

FVec2 GetMinDisplacement(const FAABB& aabb1, const FAABB& aabb2);
bool AabbsIntersect(const FAABB& left, const FAABB& right);

FxAABB ToFxAABB(const FAABB& value, gfxp::Rounding rounding = gfxp::Rounding::Nearest);
FAABB ToFAABB(const FxAABB& value);
IAABB ToIAABBFloorCeil(const FxAABB& value);
FVec2 ToFMinDisplacement(FxAABB aabb1, FxAABB aabb2);

namespace rng {

void SetSeed(std::uint32_t seed);
std::uint32_t RandomU32();
int RandomIntInclusive(int minimum, int maximum);
int RandomIntExclusive(int minimum, int maximum);
float RandomFloat(float minimum, float maximum);

} // namespace rng

} // namespace splonks

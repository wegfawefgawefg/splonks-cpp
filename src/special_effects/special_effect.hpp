#pragma once

#include "math_types.hpp"

#include <cstdint>

namespace splonks {

enum class SpecialEffectType {
    GrenadeBoom,
    BasicSmoke,
    SimpleSpark,
    Pow,
    BloodBall,
    LittleBrownShard,
};

enum class SpecialEffectSampleRegionName {
    BigExplosion,
    LittleExplosion,
    Spark,
    Pow,
    LittleSmoke,
    BigSmoke,
    ExplosionFrame1,
    ExplosionFrame2,
    ExplosionFrame3,
    ExplosionFrame4,
    BloodBall,
    LittleBrownShard,
};

struct SampleRegion {
    IVec2 pos;
    IVec2 size;
};

class SpecialEffect {
  public:
    virtual ~SpecialEffect() = default;

    virtual void Step() = 0;
    virtual bool IsFinished() const = 0;

    virtual Vec2 GetPos() const = 0;
    virtual Vec2 GetSize() const = 0;
    virtual float GetRot() const = 0;
    virtual std::uint32_t GetCounter() const = 0;
    virtual SpecialEffectType GetType() const = 0;
    virtual float GetAlpha() const = 0;
    virtual const SampleRegion& GetSampleRegion() const = 0;
};

const SampleRegion& GetSampleRegion(SpecialEffectType special_effect_type, std::uint32_t counter);
SpecialEffectSampleRegionName
GetSpecialEffectSampleRegionName(SpecialEffectType special_effect_type, std::uint32_t counter);
const SampleRegion& GetSpecialEffectSampleRegionFromName(SpecialEffectSampleRegionName name);

} // namespace splonks

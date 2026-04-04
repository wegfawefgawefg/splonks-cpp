#include "special_effects/special_effect.hpp"

namespace splonks {

const SampleRegion& GetSampleRegion(SpecialEffectType special_effect_type, std::uint32_t counter) {
    const SpecialEffectSampleRegionName region_name =
        GetSpecialEffectSampleRegionName(special_effect_type, counter);
    return GetSpecialEffectSampleRegionFromName(region_name);
}

SpecialEffectSampleRegionName
GetSpecialEffectSampleRegionName(SpecialEffectType special_effect_type, std::uint32_t counter) {
    switch (special_effect_type) {
    case SpecialEffectType::GrenadeBoom:
        if (counter >= 6 && counter <= 7) {
            return SpecialEffectSampleRegionName::ExplosionFrame1;
        }
        if (counter >= 4 && counter <= 5) {
            return SpecialEffectSampleRegionName::ExplosionFrame2;
        }
        if (counter >= 2 && counter <= 3) {
            return SpecialEffectSampleRegionName::ExplosionFrame3;
        }
        return SpecialEffectSampleRegionName::ExplosionFrame4;
    case SpecialEffectType::BasicSmoke:
        return SpecialEffectSampleRegionName::BigSmoke;
    case SpecialEffectType::SimpleSpark:
        return SpecialEffectSampleRegionName::Spark;
    case SpecialEffectType::Pow:
        return SpecialEffectSampleRegionName::Pow;
    case SpecialEffectType::BloodBall:
        return SpecialEffectSampleRegionName::BloodBall;
    case SpecialEffectType::LittleBrownShard:
        return SpecialEffectSampleRegionName::LittleBrownShard;
    }

    return SpecialEffectSampleRegionName::ExplosionFrame4;
}

const SampleRegion& GetSpecialEffectSampleRegionFromName(SpecialEffectSampleRegionName name) {
    static const SampleRegion big_explosion{IVec2::New(0, 0), IVec2::New(45, 41)};
    static const SampleRegion little_explosion{IVec2::New(60, 0), IVec2::New(10, 8)};
    static const SampleRegion spark{IVec2::New(63, 22), IVec2::New(7, 12)};
    static const SampleRegion pow{IVec2::New(61, 12), IVec2::New(9, 7)};
    static const SampleRegion little_smoke{IVec2::New(71, 0), IVec2::New(10, 10)};
    static const SampleRegion big_smoke{IVec2::New(0, 269), IVec2::New(65, 61)};
    static const SampleRegion explosion_frame1{IVec2::New(0, 164), IVec2::New(45, 42)};
    static const SampleRegion explosion_frame2{IVec2::New(0, 40), IVec2::New(62, 60)};
    static const SampleRegion explosion_frame3{IVec2::New(0, 101), IVec2::New(61, 62)};
    static const SampleRegion explosion_frame4{IVec2::New(0, 206), IVec2::New(65, 61)};
    static const SampleRegion blood_ball{IVec2::New(63, 38), IVec2::New(16, 17)};
    static const SampleRegion little_brown_shard{IVec2::New(89, 5), IVec2::New(2, 1)};

    switch (name) {
    case SpecialEffectSampleRegionName::BigExplosion:
        return big_explosion;
    case SpecialEffectSampleRegionName::LittleExplosion:
        return little_explosion;
    case SpecialEffectSampleRegionName::Spark:
        return spark;
    case SpecialEffectSampleRegionName::Pow:
        return pow;
    case SpecialEffectSampleRegionName::LittleSmoke:
        return little_smoke;
    case SpecialEffectSampleRegionName::BigSmoke:
        return big_smoke;
    case SpecialEffectSampleRegionName::ExplosionFrame1:
        return explosion_frame1;
    case SpecialEffectSampleRegionName::ExplosionFrame2:
        return explosion_frame2;
    case SpecialEffectSampleRegionName::ExplosionFrame3:
        return explosion_frame3;
    case SpecialEffectSampleRegionName::ExplosionFrame4:
        return explosion_frame4;
    case SpecialEffectSampleRegionName::BloodBall:
        return blood_ball;
    case SpecialEffectSampleRegionName::LittleBrownShard:
        return little_brown_shard;
    }

    return explosion_frame4;
}

} // namespace splonks

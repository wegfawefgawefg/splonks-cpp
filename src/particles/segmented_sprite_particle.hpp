#pragma once

#include "particles/particle_archetypes.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace splonks {

constexpr std::size_t kMaxSegmentedSpriteParticlePoints = 64;

struct SegmentedSpriteParticle {
    std::uint32_t counter = 0;
    bool finish_on_animation_end = false;
    SegmentedSpriteParticleArchetypeId archetype_id =
        kInvalidSegmentedSpriteParticleArchetypeId;
    float alpha = 1.0F;
    bool horizontal_flip = false;
    std::array<Vec2, kMaxSegmentedSpriteParticlePoints> points{};
    std::size_t point_count = 0;
    FrameDataAnimator frame_data_animator{};

    void Step(const FrameDataDb& frame_data_db, float dt);
    bool IsFinished() const;
};

} // namespace splonks

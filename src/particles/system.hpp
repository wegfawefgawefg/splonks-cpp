#pragma once

#include "math_types.hpp"
#include "particles/ribbon_particle.hpp"
#include "particles/scripted_particle.hpp"
#include "particles/segmented_sprite_particle.hpp"
#include "particles/sprite_particle.hpp"

#include <vector>

namespace splonks {

struct ParticleSystem {
    std::vector<SpriteParticle> sprite_particles;
    std::vector<ScriptedParticle> scripted_particles;
    std::vector<RibbonParticle> ribbon_particles;
    std::vector<SegmentedSpriteParticle> segmented_sprite_particles;

    void Add(const SpriteParticle& particle);
    void Add(SpriteParticle&& particle);
    void Add(const RibbonParticle& particle);
    void Add(RibbonParticle&& particle);
    void Add(const SegmentedSpriteParticle& particle);
    void Add(SegmentedSpriteParticle&& particle);
    void Add(const ScriptedParticle& particle);
    void Add(ScriptedParticle&& particle);
    void AddScripted(
        ScriptedParticleArchetypeId archetype_id,
        const Vec2& pos,
        bool horizontal_flip = false
    );
    void Step(const FrameDataDb& frame_data_db, float dt);
    void Clear();
};

} // namespace splonks

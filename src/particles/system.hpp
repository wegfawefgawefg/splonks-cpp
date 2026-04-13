#pragma once

#include "particles/particle.hpp"

#include <memory>
#include <vector>

namespace splonks {

struct ParticleSystem {
    std::vector<std::unique_ptr<Particle>> effects;

    void Add(std::unique_ptr<Particle> effect);
    void Step(const FrameDataDb& frame_data_db, float dt);
    void Clear();
};

} // namespace splonks

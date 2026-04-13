#include "particles/system.hpp"

namespace splonks {

void ParticleSystem::Add(std::unique_ptr<Particle> effect) {
    if (effect == nullptr) {
        return;
    }
    effects.push_back(std::move(effect));
}

void ParticleSystem::Step(const FrameDataDb& frame_data_db, float dt) {
    for (auto& effect : effects) {
        effect->Step(frame_data_db, dt);
    }

    std::vector<std::unique_ptr<Particle>> kept_effects;
    kept_effects.reserve(effects.size());
    for (auto& effect : effects) {
        if (!effect->IsFinished()) {
            kept_effects.push_back(std::move(effect));
        }
    }
    effects = std::move(kept_effects);
}

void ParticleSystem::Clear() {
    effects.clear();
}

} // namespace splonks

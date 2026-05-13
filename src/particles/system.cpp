#include "particles/system.hpp"

namespace splonks {

namespace {

template <typename ParticleT>
void CompactLiveParticles(std::vector<ParticleT>& particles) {
    std::size_t write_index = 0;
    for (std::size_t read_index = 0; read_index < particles.size(); ++read_index) {
        if (!particles[read_index].IsFinished()) {
            if (write_index != read_index) {
                particles[write_index] = std::move(particles[read_index]);
            }
            write_index += 1;
        }
    }
    particles.resize(write_index);
}

template <typename ParticleT>
void StepParticleFamily(std::vector<ParticleT>& particles, const AFrameDb& aframe_db, float dt) {
    for (ParticleT& particle : particles) {
        particle.Step(aframe_db, dt);
    }
    CompactLiveParticles(particles);
}

} // namespace

void ParticleSystem::Add(const SpriteParticle& particle) {
    sprite_particles.push_back(particle);
}

void ParticleSystem::Add(SpriteParticle&& particle) {
    sprite_particles.push_back(std::move(particle));
}

void ParticleSystem::Add(const RibbonParticle& particle) {
    ribbon_particles.push_back(particle);
}

void ParticleSystem::Add(RibbonParticle&& particle) {
    ribbon_particles.push_back(std::move(particle));
}

void ParticleSystem::Add(const SegmentedSpriteParticle& particle) {
    segmented_sprite_particles.push_back(particle);
}

void ParticleSystem::Add(SegmentedSpriteParticle&& particle) {
    segmented_sprite_particles.push_back(std::move(particle));
}

void ParticleSystem::Add(const ScriptedParticle& particle) {
    scripted_particles.push_back(particle);
}

void ParticleSystem::Add(ScriptedParticle&& particle) {
    scripted_particles.push_back(std::move(particle));
}

void ParticleSystem::AddScripted(
    ScriptedParticleSpecId spec_id,
    const Vec2& pos,
    bool horizontal_flip
) {
    scripted_particles.push_back(MakeScriptedParticle(spec_id, pos, horizontal_flip));
}

void ParticleSystem::Step(const AFrameDb& aframe_db, float dt) {
    StepParticleFamily(sprite_particles, aframe_db, dt);
    StepParticleFamily(scripted_particles, aframe_db, dt);
    StepParticleFamily(ribbon_particles, aframe_db, dt);
    StepParticleFamily(segmented_sprite_particles, aframe_db, dt);
}

void ParticleSystem::Clear() {
    sprite_particles.clear();
    scripted_particles.clear();
    ribbon_particles.clear();
    segmented_sprite_particles.clear();
}

} // namespace splonks

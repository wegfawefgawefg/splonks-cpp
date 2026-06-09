#include "ents/sac_altar_topper.hpp"

#include "ents/sac_altar.hpp"
#include "ents/common/common.hpp"
#include "ent/spec.hpp"
#include "aframe_id.hpp"
#include "math_types.hpp"
#include "particles/sprite_particle.hpp"
#include "state.hpp"
#include "utils.hpp"

#include <memory>

namespace splonks::ents::sac_altar_topper {

namespace {

constexpr float kIdleSmokeIntervalFrames = 24.0F;

void SpawnTopperSmoke(State& state, const Vec2& pos, float scale_bias) {
    SpriteParticle smoke{};
    smoke.aframe_animator = AFrameAnimator::New(aframe_ids::LittleSmoke);
    smoke.draw_layer = DrawLayer::Foreground;
    smoke.counter = static_cast<std::uint32_t>(rng::RandomIntExclusive(18, 30));
    smoke.pos = pos + Vec2::New(rng::RandomFloat(-2.0F, 2.0F), rng::RandomFloat(-1.0F, 1.0F));
    const float size = rng::RandomFloat(4.0F + scale_bias, 7.0F + scale_bias);
    smoke.size = Vec2::New(size, size);
    smoke.rot = rng::RandomFloat(0.0F, 360.0F);
    smoke.alpha = rng::RandomFloat(0.55F, 0.85F);
    smoke.vel = Vec2::New(rng::RandomFloat(-0.08F, 0.08F), rng::RandomFloat(-0.45F, -0.18F));
    smoke.svel = Vec2::New(rng::RandomFloat(0.01F, 0.03F), rng::RandomFloat(0.01F, 0.03F));
    smoke.rotvel = rng::RandomFloat(-0.2F, 0.2F);
    smoke.alpha_vel = -0.02F;
    smoke.acc = Vec2::New(0.0F, -0.005F);
    smoke.sacc = Vec2::New(0.0F, 0.0F);
    smoke.rotacc = 0.0F;
    smoke.alpha_acc = -0.003F;
    state.particles.Add(std::move(smoke));
}

void StepEntLogicAsSacAltarTopper(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)audio;
    (void)dt;

    if (ent_idx >= state.ents.ents.size()) {
        return;
    }

    Ent& topper = state.ents.ents[ent_idx];
    if (!topper.active) {
        return;
    }

    if (topper.aframe_animator.anim_id == aframe_ids::SacAltarSac) {
        if (topper.counter_b > sim::Scalar::zero()) {
            topper.counter_b -= sim::Scalar::from_int(1);
        }
        if (topper.counter_b <= sim::Scalar::zero()) {
            SetAnim(topper, aframe_ids::SacAltarTopper);
            topper.aframe_animator.loop = true;
            topper.aframe_animator.animate = true;
            topper.aframe_animator.finished = false;
        }
    }

    if (topper.counter_a > sim::Scalar::zero()) {
        topper.counter_a -= sim::Scalar::from_int(1);
    }
    if (topper.counter_a <= sim::Scalar::zero()) {
        topper.counter_a = sim::ToSimScalar(kIdleSmokeIntervalFrames);
        const sim::Vec2 emit_pos =
            ents::common::GetEmitPointForEnt(topper, graphics, topper.GetSimCenter());
        SpawnTopperSmoke(state, sim::ToRenderVec2(emit_pos), 0.0F);
    }
}

} // namespace

extern const EntSpec kSacAltarTopperSpec{
    .type_ = EntType::SacAltarTopper,
    .size = EntSpecSize(28.0F, 16.0F),
    .health = 1,
    .has_physics = false,
    .can_collide = false,
    .can_be_picked_up = false,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Background,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::CrushingSpikesAndExplosion,
    .on_death = ents::sac_altar::OnDeathAsSacAltarPiece,
    .step_logic = StepEntLogicAsSacAltarTopper,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::SacAltarTopper),
};

} // namespace splonks::ents::sac_altar_topper

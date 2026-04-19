#include "entities/sac_altar_topper.hpp"

#include "entities/sac_altar.hpp"
#include "entities/common/common.hpp"
#include "entity/archetype.hpp"
#include "frame_data_id.hpp"
#include "math_types.hpp"
#include "particles/ultra_dynamic_particle.hpp"
#include "state.hpp"
#include "utils.hpp"

#include <memory>

namespace splonks::entities::sac_altar_topper {

namespace {

constexpr float kIdleSmokeIntervalFrames = 24.0F;

void SpawnTopperSmoke(State& state, const Vec2& pos, float scale_bias) {
    auto smoke = std::make_unique<UltraDynamicParticle>();
    smoke->frame_data_animator = FrameDataAnimator::New(frame_data_ids::LittleSmoke);
    smoke->draw_layer = DrawLayer::Foreground;
    smoke->counter = static_cast<std::uint32_t>(rng::RandomIntExclusive(18, 30));
    smoke->pos = pos + Vec2::New(rng::RandomFloat(-2.0F, 2.0F), rng::RandomFloat(-1.0F, 1.0F));
    const float size = rng::RandomFloat(4.0F + scale_bias, 7.0F + scale_bias);
    smoke->size = Vec2::New(size, size);
    smoke->rot = rng::RandomFloat(0.0F, 360.0F);
    smoke->alpha = rng::RandomFloat(0.55F, 0.85F);
    smoke->vel = Vec2::New(rng::RandomFloat(-0.08F, 0.08F), rng::RandomFloat(-0.45F, -0.18F));
    smoke->svel = Vec2::New(rng::RandomFloat(0.01F, 0.03F), rng::RandomFloat(0.01F, 0.03F));
    smoke->rotvel = rng::RandomFloat(-0.2F, 0.2F);
    smoke->alpha_vel = -0.02F;
    smoke->acc = Vec2::New(0.0F, -0.005F);
    smoke->sacc = Vec2::New(0.0F, 0.0F);
    smoke->rotacc = 0.0F;
    smoke->alpha_acc = -0.003F;
    state.particles.Add(std::move(smoke));
}

void StepEntityLogicAsSacAltarTopper(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)audio;
    (void)dt;

    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    Entity& topper = state.entity_manager.entities[entity_idx];
    if (!topper.active) {
        return;
    }

    if (topper.frame_data_animator.animation_id == frame_data_ids::SacAltarSac) {
        if (topper.counter_b > 0.0F) {
            topper.counter_b -= 1.0F;
        }
        if (topper.counter_b <= 0.0F) {
            SetAnimation(topper, frame_data_ids::SacAltarTopper);
            topper.frame_data_animator.loop = true;
            topper.frame_data_animator.animate = true;
            topper.frame_data_animator.finished = false;
        }
    }

    if (topper.counter_a > 0.0F) {
        topper.counter_a -= 1.0F;
    }
    if (topper.counter_a <= 0.0F) {
        topper.counter_a = kIdleSmokeIntervalFrames;
        const Vec2 emit_pos = entities::common::GetEmitPointForEntity(topper, graphics, topper.GetCenter());
        SpawnTopperSmoke(state, emit_pos, 0.0F);
    }
}

} // namespace

extern const EntityArchetype kSacAltarTopperArchetype{
    .type_ = EntityType::SacAltarTopper,
    .size = Vec2::New(28.0F, 16.0F),
    .health = 1,
    .has_physics = false,
    .can_collide = false,
    .can_be_picked_up = false,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Background,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::CrushingSpikesAndExplosion,
    .on_death = entities::sac_altar::OnDeathAsSacAltarPiece,
    .step_logic = StepEntityLogicAsSacAltarTopper,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::SacAltarTopper),
};

} // namespace splonks::entities::sac_altar_topper

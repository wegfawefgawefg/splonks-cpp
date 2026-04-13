#include "entities/pistol.hpp"

#include "audio.hpp"
#include "entity/archetype.hpp"
#include "entities/common/common.hpp"
#include "frame_data_id.hpp"
#include "graphics.hpp"
#include "hitscan.hpp"
#include "state.hpp"
#include "utils.hpp"

#include "particles/dynamic_particle.hpp"
#include "particles/ultra_dynamic_particle.hpp"

#include <memory>

namespace splonks::entities::pistol {

namespace {

constexpr float kPistolFireCooldownFrames = 12.0F;
constexpr float kPistolAmmo = 4.0F;
constexpr unsigned int kPistolDamage = 4;

void SpawnPistolMuzzleSmoke(State& state, const Vec2& pos, int direction) {
    for (int i = 0; i < 4; ++i) {
        auto effect = std::make_unique<UltraDynamicParticle>();
        effect->frame_data_animator = FrameDataAnimator::New(frame_data_ids::LittleSmoke);
        effect->draw_layer = DrawLayer::Foreground;
        effect->counter = static_cast<std::uint32_t>(rng::RandomIntInclusive(8, 14));
        effect->pos = pos + Vec2::New(rng::RandomFloat(-1.0F, 1.0F), rng::RandomFloat(-1.0F, 1.0F));
        effect->size = Vec2::New(rng::RandomFloat(3.0F, 5.0F), rng::RandomFloat(3.0F, 5.0F));
        effect->rot = rng::RandomFloat(0.0F, 360.0F);
        effect->alpha = rng::RandomFloat(0.75F, 0.95F);
        effect->vel = Vec2::New(
            rng::RandomFloat(0.05F, 0.25F) * static_cast<float>(direction),
            rng::RandomFloat(-0.18F, -0.04F)
        );
        effect->svel = Vec2::New(rng::RandomFloat(0.08F, 0.20F), rng::RandomFloat(0.08F, 0.20F));
        effect->rotvel = rng::RandomFloat(-1.5F, 1.5F);
        effect->alpha_vel = -0.05F;
        effect->acc = Vec2::New(0.0F, -0.01F);
        effect->sacc = Vec2::New(0.01F, 0.01F);
        effect->rotacc = 0.0F;
        effect->alpha_acc = -0.003F;
        state.particles.Add(std::move(effect));
    }
}

void SpawnPistolImpactEffect(State& state, const Vec2& pos, int direction) {
    for (int i = 0; i < 3; ++i) {
        auto spark = std::make_unique<DynamicParticle>();
        spark->frame_data_animator = FrameDataAnimator::New(frame_data_ids::Spark);
        spark->draw_layer = DrawLayer::Foreground;
        spark->counter = static_cast<std::uint32_t>(rng::RandomIntInclusive(5, 9));
        spark->pos = pos + Vec2::New(rng::RandomFloat(-1.0F, 1.0F), rng::RandomFloat(-1.0F, 1.0F));
        spark->size = Vec2::New(rng::RandomFloat(3.0F, 5.0F), rng::RandomFloat(4.0F, 6.0F));
        spark->rot = rng::RandomFloat(0.0F, 360.0F);
        spark->alpha = 1.0F;
        spark->vel = Vec2::New(
            rng::RandomFloat(-0.45F, -0.10F) * static_cast<float>(direction),
            rng::RandomFloat(-0.18F, 0.18F)
        );
        spark->svel = Vec2::New(-0.12F, -0.12F);
        spark->rotvel = rng::RandomFloat(-6.0F, 6.0F);
        spark->alpha_vel = -0.14F;
        state.particles.Add(std::move(spark));
    }

    for (int i = 0; i < 2; ++i) {
        auto smoke = std::make_unique<UltraDynamicParticle>();
        smoke->frame_data_animator = FrameDataAnimator::New(frame_data_ids::LittleSmoke);
        smoke->draw_layer = DrawLayer::Foreground;
        smoke->counter = static_cast<std::uint32_t>(rng::RandomIntInclusive(10, 16));
        smoke->pos = pos + Vec2::New(rng::RandomFloat(-1.0F, 1.0F), rng::RandomFloat(-1.0F, 1.0F));
        smoke->size = Vec2::New(rng::RandomFloat(3.0F, 5.0F), rng::RandomFloat(3.0F, 5.0F));
        smoke->rot = rng::RandomFloat(0.0F, 360.0F);
        smoke->alpha = rng::RandomFloat(0.75F, 0.95F);
        smoke->vel = Vec2::New(rng::RandomFloat(-0.08F, 0.08F), rng::RandomFloat(-0.18F, -0.06F));
        smoke->svel = Vec2::New(rng::RandomFloat(0.06F, 0.14F), rng::RandomFloat(0.06F, 0.14F));
        smoke->rotvel = rng::RandomFloat(-1.5F, 1.5F);
        smoke->alpha_vel = -0.05F;
        smoke->acc = Vec2::New(0.0F, -0.01F);
        smoke->sacc = Vec2::New(0.01F, 0.01F);
        smoke->rotacc = 0.0F;
        smoke->alpha_acc = -0.003F;
        state.particles.Add(std::move(smoke));
    }
}

Vec2 GetFallbackMuzzlePos(const Entity& pistol) {
    const int direction = pistol.facing == LeftOrRight::Left ? -1 : 1;
    return pistol.GetCenter() + Vec2::New(8.0F * static_cast<float>(direction), 1.0F);
}

void FirePistolShot(std::size_t entity_idx, State& state, Graphics& graphics, Audio& audio) {
    const Entity& pistol = state.entity_manager.entities[entity_idx];
    const int direction = pistol.facing == LeftOrRight::Left ? -1 : 1;
    const Vec2 muzzle_pos = common::GetEmitPointForEntity(
        pistol,
        graphics,
        GetFallbackMuzzlePos(pistol)
    );
    const int max_distance = static_cast<int>(state.stage.GetStageDims().x);
    const std::optional<VID> owner_vid = pistol.held_by_vid.has_value() ? pistol.held_by_vid
                                                                        : pistol.use_state.user_vid;

    audio.PlaySoundEffect(SoundEffect::PistolFire);
    SpawnPistolMuzzleSmoke(state, muzzle_pos, direction);

    const HitscanHit hit = TraceHitscan(
        pistol,
        muzzle_pos,
        direction,
        max_distance,
        state,
        graphics,
        owner_vid
    );
    if (hit.type == HitscanHitType::Tile ||
        hit.type == HitscanHitType::Entity) {
        SpawnPistolImpactEffect(state, ToVec2(hit.point), direction);
    }
    if (hit.type == HitscanHitType::Entity && hit.entity_vid.has_value()) {
        common::TryDamageEntity(hit.entity_vid->id, state, audio, DamageType::Attack, kPistolDamage);
    }
}

} // namespace

void OnUseAsPistol(std::size_t entity_idx, State& state, Graphics& graphics, Audio& audio) {
    Entity& pistol = state.entity_manager.entities[entity_idx];
    if (!pistol.use_state.pressed || pistol.counter_a > 0.0F) {
        return;
    }

    if (pistol.counter_b <= 0.0F) {
        audio.PlaySoundEffect(SoundEffect::GunEmpty);
        if (pistol.use_state.source == AttachmentMode::None) {
            StopUsingEntity(pistol);
        }
        return;
    }

    pistol.counter_a = kPistolFireCooldownFrames;
    pistol.counter_b -= 1.0F;
    FirePistolShot(entity_idx, state, graphics, audio);

    if (pistol.use_state.source == AttachmentMode::None) {
        StopUsingEntity(pistol);
    }
}

void StepEntityLogicAsPistol(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    (void)dt;
    Entity& pistol = state.entity_manager.entities[entity_idx];
    if (pistol.counter_a > 0.0F) {
        pistol.counter_a -= 1.0F;
        if (pistol.counter_a < 0.0F) {
            pistol.counter_a = 0.0F;
        }
    }
}

extern const EntityArchetype kPistolArchetype{
    .type_ = EntityType::Pistol,
    .size = Vec2::New(16.0F, 16.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .counter_b = kPistolAmmo,
    .damage_vulnerability = DamageVulnerability::Vulnerable,
    .on_use = OnUseAsPistol,
    .step_logic = StepEntityLogicAsPistol,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::Pistol),
};

} // namespace splonks::entities::pistol

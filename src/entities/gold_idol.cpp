#include "entities/gold_idol.hpp"

#include "audio.hpp"
#include "entities/basic_exit.hpp"
#include "entities/common/common.hpp"
#include "entities/shop.hpp"
#include "entity/archetype.hpp"
#include "entity/core_types.hpp"
#include "frame_data_animator.hpp"
#include "frame_data_id.hpp"
#include "math_types.hpp"
#include "particles/ultra_dynamic_particle.hpp"
#include "state.hpp"
#include "world_query.hpp"

#include <cstdint>
#include <memory>

namespace splonks::entities::gold_idol {

namespace {

constexpr std::uint32_t kGoldIdolExitValue = 5000;
constexpr std::uint32_t kGoldIdolShopValue = 10000;
constexpr float kRewardParticleYOffsetFactor = 0.25F;
constexpr float kRewardParticleFloatSpeed = -0.18F;
constexpr std::uint32_t kRewardParticleLifetimeFrames = 48;

void SpawnRewardParticle(State& state, const Vec2& pos, FrameDataId animation_id, const Vec2& size) {
    auto particle = std::make_unique<UltraDynamicParticle>();
    particle->frame_data_animator = FrameDataAnimator::New(animation_id);
    particle->draw_layer = DrawLayer::Foreground;
    particle->counter = kRewardParticleLifetimeFrames;
    particle->pos = pos;
    particle->size = size;
    particle->rot = 0.0F;
    particle->alpha = 1.0F;
    particle->vel = Vec2::New(0.0F, kRewardParticleFloatSpeed);
    particle->svel = Vec2::New(0.0F, 0.0F);
    particle->rotvel = 0.0F;
    particle->alpha_vel = -0.01F;
    particle->acc = Vec2::New(0.0F, 0.0F);
    particle->sacc = Vec2::New(0.0F, 0.0F);
    particle->rotacc = 0.0F;
    particle->alpha_acc = 0.0F;
    state.particles.Add(std::move(particle));
}

std::optional<VID> GetRewardTargetVid(const Entity& idol, const State& state) {
    if (state.player_vid.has_value()) {
        const Entity* const player = state.entity_manager.GetEntity(*state.player_vid);
        if (player != nullptr && player->active && player->condition != EntityCondition::Dead) {
            return player->vid;
        }
    }
    if (idol.held_by_vid.has_value()) {
        const Entity* const holder = state.entity_manager.GetEntity(*idol.held_by_vid);
        if (holder != nullptr && holder->active && holder->condition != EntityCondition::Dead) {
            return holder->vid;
        }
    }
    return std::nullopt;
}

Vec2 GetRewardParticlePosForTarget(std::optional<VID> target_vid, const State& state, const Entity& idol) {
    if (!target_vid.has_value()) {
        return idol.GetCenter();
    }

    const Entity* const target = state.entity_manager.GetEntity(*target_vid);
    if (target == nullptr || !target->active) {
        return idol.GetCenter();
    }

    const AABB target_aabb = target->GetAABB();
    return Vec2::New(
        (target_aabb.tl.x + target_aabb.br.x) * 0.5F,
        target_aabb.tl.y + target->size.y * kRewardParticleYOffsetFactor
    );
}

std::optional<std::size_t> FindIntersectingShopIdx(const Entity& idol, const State& state) {
    const AABB idol_aabb = idol.GetAABB();
    for (std::size_t entity_idx = 0; entity_idx < state.entity_manager.entities.size(); ++entity_idx) {
        const Entity& entity = state.entity_manager.entities[entity_idx];
        if (!entity.active || entity.type_ != EntityType::Shop) {
            continue;
        }
        if (WorldAabbsIntersect(state.stage, idol_aabb, shop::GetShopArea(entity))) {
            return entity_idx;
        }
    }
    return std::nullopt;
}

void SpawnGoldIdolRewardParticles(std::optional<VID> target_vid, State& state, const Entity& idol) {
    const Vec2 base_pos = GetRewardParticlePosForTarget(target_vid, state, idol);
    SpawnRewardParticle(state, base_pos + Vec2::New(-4.0F, 0.0F), frame_data_ids::BigGoldStack, Vec2::New(12.0F, 12.0F));
    SpawnRewardParticle(state, base_pos + Vec2::New(5.0F, -2.0F), frame_data_ids::GoldBars, Vec2::New(12.0F, 12.0F));
}

void AwardMoneyToTarget(std::optional<VID> target_vid, std::uint32_t amount, State& state) {
    if (!target_vid.has_value() || amount == 0) {
        return;
    }
    Entity* const target = state.entity_manager.GetEntityMut(*target_vid);
    if (target == nullptr || !target->active || target->condition == EntityCondition::Dead) {
        return;
    }
    target->money += amount;
}

void RedeemGoldIdol(
    std::size_t entity_idx,
    std::uint32_t amount,
    SoundEffect sound_effect,
    State& state,
    Graphics& graphics,
    Audio& audio
) {
    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    Entity& idol = state.entity_manager.entities[entity_idx];
    if (!idol.active) {
        return;
    }

    const std::optional<VID> reward_target_vid = GetRewardTargetVid(idol, state);
    SpawnGoldIdolRewardParticles(reward_target_vid, state, idol);
    AwardMoneyToTarget(reward_target_vid, amount, state);
    audio.PlaySoundEffect(sound_effect);

    common::ReleaseEntityFromHolder(idol, state);
    idol.damage_vulnerability = DamageVulnerability::Immune;
    idol.can_collide = false;
    idol.has_physics = false;
    state.entity_manager.SetInactive(entity_idx);
    state.UpdateSidForEntity(entity_idx, graphics);
}

} // namespace

void StepEntityLogicAsGoldIdol(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)dt;

    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    const Entity& idol = state.entity_manager.entities[entity_idx];
    if (!idol.active || idol.condition == EntityCondition::Dead) {
        return;
    }

    if (idol.grounded && !idol.held_by_vid.has_value()) {
        if (const std::optional<std::size_t> shop_idx = FindIntersectingShopIdx(idol, state)) {
            (void)shop_idx;
            RedeemGoldIdol(entity_idx, kGoldIdolShopValue, SoundEffect::CashRegister, state, graphics, audio);
            return;
        }
    }

    if (entities::basic_exit::IsEntityTouchingBasicExit(idol, state, graphics)) {
        RedeemGoldIdol(entity_idx, kGoldIdolExitValue, SoundEffect::GoldStack, state, graphics, audio);
        return;
    }
}

extern const EntityArchetype kGoldIdolArchetype{
    .type_ = EntityType::GoldIdol,
    .size = Vec2::New(12.0F, 12.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::CrushingOnly,
    .step_logic = StepEntityLogicAsGoldIdol,
    .alignment = Alignment::Neutral,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::GoldIdol),
};

} // namespace splonks::entities::gold_idol

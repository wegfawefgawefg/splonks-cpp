#include "ents/gold_idol.hpp"

#include "audio.hpp"
#include "ents/basic_exit.hpp"
#include "ents/common/common.hpp"
#include "ents/shop.hpp"
#include "ent/spec.hpp"
#include "ent/core_types.hpp"
#include "aframe_animator.hpp"
#include "aframe_id.hpp"
#include "math_types.hpp"
#include "particles/sprite_particle.hpp"
#include "player_queries.hpp"
#include "state.hpp"
#include "world_ops.hpp"
#include "world_query.hpp"

#include <cstdint>
#include <memory>

namespace splonks::ents::gold_idol {

namespace {

constexpr std::uint32_t kGoldIdolExitValue = 5000;
constexpr std::uint32_t kGoldIdolShopValue = 10000;
constexpr float kRewardParticleYOffsetFactor = 0.25F;
constexpr float kRewardParticleFloatSpeed = -0.18F;
constexpr std::uint32_t kRewardParticleLifetimeFrames = 48;

void SpawnRewardParticle(State& state, const Vec2& pos, AFrameId anim_id, const Vec2& size) {
    SpriteParticle particle{};
    particle.aframe_animator = AFrameAnimator::New(anim_id);
    particle.draw_layer = DrawLayer::Foreground;
    particle.counter = kRewardParticleLifetimeFrames;
    particle.pos = pos;
    particle.size = size;
    particle.rot = 0.0F;
    particle.alpha = 1.0F;
    particle.vel = Vec2::New(0.0F, kRewardParticleFloatSpeed);
    particle.svel = Vec2::New(0.0F, 0.0F);
    particle.rotvel = 0.0F;
    particle.alpha_vel = -0.01F;
    particle.acc = Vec2::New(0.0F, 0.0F);
    particle.sacc = Vec2::New(0.0F, 0.0F);
    particle.rotacc = 0.0F;
    particle.alpha_acc = 0.0F;
    state.particles.Add(std::move(particle));
}

std::optional<VID> GetRewardTargetVid(const Ent& idol, const State& state) {
    if (idol.held_by_vid.has_value()) {
        const Ent* const holder = state.ents.GetEnt(*idol.held_by_vid);
        if (holder != nullptr && holder->active && holder->condition != EntCondition::Dead) {
            return holder->vid;
        }
    }
    return FindNearestPlayerVid(state, idol.GetCenter(), false);
}

Vec2 GetRewardParticlePosForTarget(std::optional<VID> target_vid, const State& state, const Ent& idol) {
    if (!target_vid.has_value()) {
        return sim::ToRenderVec2(idol.GetSimCenter());
    }

    const Ent* const target = state.ents.GetEnt(*target_vid);
    if (target == nullptr || !target->active) {
        return sim::ToRenderVec2(idol.GetSimCenter());
    }

    const sim::AABB target_aabb = target->GetSimAABB();
    return sim::ToRenderVec2(sim::Vec2{
        target_aabb.center().x,
        target_aabb.tl.y + target->size.y * sim::ToSimScalar(kRewardParticleYOffsetFactor),
    });
}

std::optional<std::size_t> FindIntersectingShopIdx(const Ent& idol, const State& state) {
    const sim::AABB idol_aabb = idol.GetSimAABB();
    for (std::size_t ent_idx = 0; ent_idx < state.ents.ents.size(); ++ent_idx) {
        const Ent& ent = state.ents.ents[ent_idx];
        if (!ent.active || ent.type_ != EntType::Shop) {
            continue;
        }
        if (WorldAabbsIntersect(state.stage, idol_aabb, shop::GetShopArea(ent))) {
            return ent_idx;
        }
    }
    return std::nullopt;
}

void SpawnGoldIdolRewardParticles(std::optional<VID> target_vid, State& state, const Ent& idol) {
    const Vec2 base_pos = GetRewardParticlePosForTarget(target_vid, state, idol);
    SpawnRewardParticle(state, base_pos + Vec2::New(-4.0F, 0.0F), aframe_ids::BigGoldStack, Vec2::New(12.0F, 12.0F));
    SpawnRewardParticle(state, base_pos + Vec2::New(5.0F, -2.0F), aframe_ids::GoldBars, Vec2::New(12.0F, 12.0F));
}

void AwardMoneyToTarget(std::optional<VID> target_vid, std::uint32_t amount, State& state) {
    if (!target_vid.has_value() || amount == 0) {
        return;
    }
    Ent* const target = state.ents.GetEntMut(*target_vid);
    if (target == nullptr || !target->active || target->condition == EntCondition::Dead) {
        return;
    }
    target->money += amount;
}

void RedeemGoldIdol(
    std::size_t ent_idx,
    std::uint32_t amount,
    AudioAssetId sound_effect,
    State& state,
    Graphics& graphics,
    Audio& audio
) {
    (void)audio;
    if (ent_idx >= state.ents.ents.size()) {
        return;
    }

    Ent& idol = state.ents.ents[ent_idx];
    if (!idol.active) {
        return;
    }

    const std::optional<VID> reward_target_vid = GetRewardTargetVid(idol, state);
    SpawnGoldIdolRewardParticles(reward_target_vid, state, idol);
    AwardMoneyToTarget(reward_target_vid, amount, state);
    (void)PlayEntCenterSoundEmitter(state, idol, sound_effect);

    common::ReleaseEntFromHolder(idol, state);
    idol.damage_vuln = DamageVuln::Immune;
    idol.can_collide = false;
    idol.has_physics = false;
    (void)world_ops::DeactivateEnt(state, idol.vid);
    state.UpdateSidForEnt(ent_idx, graphics);
}

} // namespace

void StepEntLogicAsGoldIdol(
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

    const Ent& idol = state.ents.ents[ent_idx];
    if (!idol.active || idol.condition == EntCondition::Dead) {
        return;
    }

    if (idol.grounded && !idol.held_by_vid.has_value()) {
        if (const std::optional<std::size_t> shop_idx = FindIntersectingShopIdx(idol, state)) {
            (void)shop_idx;
            const std::uint32_t amount =
                idol.counter_b > 0.0F ? static_cast<std::uint32_t>(idol.counter_b) : kGoldIdolShopValue;
            RedeemGoldIdol(ent_idx, amount, audio_asset_ids::CashRegister, state, graphics, audio);
            return;
        }
    }

    if (ents::basic_exit::IsEntTouchingBasicExit(idol, state, graphics)) {
        const std::uint32_t amount =
            idol.counter_a > 0.0F ? static_cast<std::uint32_t>(idol.counter_a) : kGoldIdolExitValue;
        RedeemGoldIdol(ent_idx, amount, audio_asset_ids::GoldStack, state, graphics, audio);
        return;
    }
}

extern const EntSpec kGoldIdolSpec{
    .type_ = EntType::GoldIdol,
    .size = EntSpecSize(12.0F, 12.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .counter_a = EntSpecCounter(static_cast<float>(kGoldIdolExitValue)),
    .counter_b = EntSpecCounter(static_cast<float>(kGoldIdolShopValue)),
    .damage_vuln = DamageVuln::CrushingOnly,
    .proj_contact_damage_amount = 0,
    .step_logic = StepEntLogicAsGoldIdol,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::GoldIdol),
};

} // namespace splonks::ents::gold_idol

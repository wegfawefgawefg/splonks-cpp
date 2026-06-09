#include "ents/spider.hpp"

#include "ent/spec.hpp"
#include "ents/common/common.hpp"
#include "aframe_animator.hpp"
#include "aframe_id.hpp"
#include "on_damage_effects.hpp"
#include "player_queries.hpp"
#include "utils.hpp"
#include "world_ops.hpp"
#include "world_query.hpp"

#include <cmath>

namespace splonks::ents::spider {

namespace {

constexpr int kPassiveSpiderCooldownMinFrames = 25;
constexpr int kPassiveSpiderCooldownMaxFrames = 60;
constexpr float kPassiveSpiderHopSpeedX = 1.25F;

constexpr int kRageSpiderAggroDistance = 90;
constexpr int kGiantSpiderAggroDistance = 120;
constexpr int kAggroSpiderCooldownMinFrames = 5;
constexpr int kAggroSpiderCooldownMaxFrames = 20;
constexpr float kRageSpiderHopSpeedX = 2.5F;
constexpr float kGiantSpiderHopSpeedX = 2.5F;
constexpr float kSpiderIdleSpeedThreshold = 0.1F;

std::optional<sim::Vec2> GetNearestPlayerDelta(const Ent& ent, const State& state) {
    const Ent* const player = FindNearestPlayer(state, ent.GetSimCenter(), false);
    if (player == nullptr || player->condition == EntCondition::Dead) {
        return std::nullopt;
    }

    const sim::Vec2 ent_center = ent.GetSimCenter();
    const sim::Vec2 player_center = GetNearestWorldPoint(state.stage, ent_center, player->GetSimCenter());
    return player_center - ent_center;
}

void SpawnGiantSpiderLoot(sim::Vec2 center, State& state) {
    const int gem_count = state.drng.RandomIntInclusive(1, 3);
    for (int i = 0; i < gem_count; ++i) {
        EntType gem_type = EntType::EmeraldBig;
        switch (state.drng.RandomIntInclusive(1, 3)) {
        case 1:
            gem_type = EntType::EmeraldBig;
            break;
        case 2:
            gem_type = EntType::SapphireBig;
            break;
        case 3:
            gem_type = EntType::RubyBig;
            break;
        }

        if (world_ops::SpawnEnt(state, gem_type, [&](Ent& gem) {
                gem.SetSimCenter(center);
                gem.vel = sim::Vec2{
                    RandomSimScalar(state.drng, sim::Scalar::from_int(-2), sim::Scalar::from_int(2)),
                    sim::Scalar::from_int(-2),
                };
            }) == nullptr) {
            continue;
        }
    }

    (void)world_ops::SpawnEnt(state, EntType::Paste, [&](Ent& paste) {
        paste.SetSimCenter(center);
        paste.vel = sim::Vec2::zero();
    });
}

void HandleGiantSpiderDeath(std::size_t ent_idx, State& state, Audio& audio) {
    (void)audio;

    if (ent_idx >= state.ents.ents.size()) {
        return;
    }

    const Ent& giant_spider = state.ents.ents[ent_idx];
    const sim::Vec2 center = giant_spider.GetSimCenter();
    SpawnDamageEffectAnimBurst(aframe_ids::BloodBall, sim::ToRenderVec2(center), state);
    SpawnGiantSpiderLoot(center, state);
}

void FaceTowardNearestPlayer(Ent& ent, const State& state) {
    const std::optional<sim::Vec2> player_delta = GetNearestPlayerDelta(ent, state);
    if (!player_delta.has_value()) {
        return;
    }

    if (player_delta->x < sim::Scalar::zero()) {
        ent.facing = Side::Left;
    } else if (player_delta->x > sim::Scalar::zero()) {
        ent.facing = Side::Right;
    }
}

void StepPassiveSpider(Ent& ent, State& state) {
    if (ent.condition != EntCondition::Normal) {
        return;
    }

    TrySetAnim(ent, EntDisplayState::Neutral);
    if (!ent.grounded) {
        return;
    }

    if (ent.counter_a > sim::Scalar::zero()) {
        ent.counter_a -= sim::Scalar::from_int(1);
        if (ent.vel.x.abs() < sim::ToSimScalar(kSpiderIdleSpeedThreshold)) {
            ent.vel.x = sim::Scalar::zero();
        }
        return;
    }

    if (state.drng.RandomIntInclusive(0, 1) == 0) {
        ent.facing = ent.facing == Side::Left ? Side::Right : Side::Left;
    }

    ent.vel.y = -sim::Scalar::from_int(state.drng.RandomIntInclusive(2, 4));
    ent.vel.x = sim::ToSimScalar(
        ent.facing == Side::Left ? -kPassiveSpiderHopSpeedX : kPassiveSpiderHopSpeedX
    );
    ent.counter_a = sim::Scalar::from_int(state.drng.RandomIntInclusive(
        kPassiveSpiderCooldownMinFrames,
        kPassiveSpiderCooldownMaxFrames
    ));
}

void TryHopTowardPlayer(
    Ent& ent,
    State& state,
    int aggro_distance,
    float hop_speed_x,
    int hop_speed_y_min,
    int hop_speed_y_max
) {
    const std::optional<sim::Vec2> player_delta = GetNearestPlayerDelta(ent, state);
    if (!player_delta.has_value() ||
        gfxp::length_sq(*player_delta) > sim::Scalar::from_int(aggro_distance * aggro_distance)) {
        ent.counter_a = sim::Scalar::from_int(state.drng.RandomIntInclusive(
            kAggroSpiderCooldownMinFrames,
            kAggroSpiderCooldownMaxFrames
        ));
        return;
    }

    FaceTowardNearestPlayer(ent, state);
    ent.vel.y = -sim::Scalar::from_int(state.drng.RandomIntInclusive(hop_speed_y_min, hop_speed_y_max));
    ent.vel.x = sim::ToSimScalar(ent.facing == Side::Left ? -hop_speed_x : hop_speed_x);
    ent.counter_a = sim::Scalar::from_int(state.drng.RandomIntInclusive(
        kAggroSpiderCooldownMinFrames,
        kAggroSpiderCooldownMaxFrames
    ));
}

void StepAggroSpider(
    Ent& ent,
    State& state,
    int aggro_distance,
    float hop_speed_x,
    int hop_speed_y_min,
    int hop_speed_y_max
) {
    if (ent.condition != EntCondition::Normal) {
        return;
    }

    TrySetAnim(ent, EntDisplayState::Neutral);
    if (!ent.grounded) {
        return;
    }

    if (ent.counter_a > sim::Scalar::zero()) {
        ent.counter_a -= sim::Scalar::from_int(1);
        if (ent.vel.x.abs() < sim::ToSimScalar(kSpiderIdleSpeedThreshold)) {
            ent.vel.x = sim::Scalar::zero();
        }
        return;
    }

    TryHopTowardPlayer(ent, state, aggro_distance, hop_speed_x, hop_speed_y_min, hop_speed_y_max);
}

} // namespace

void OnDeathAsGiantSpider(std::size_t ent_idx, State& state, Audio& audio) {
    HandleGiantSpiderDeath(ent_idx, state, audio);
}

void StepEntLogicAsSpider(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    (void)dt;

    Ent& spider = state.ents.ents[ent_idx];
    StepPassiveSpider(spider, state);
}

void StepEntLogicAsRageSpider(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    (void)dt;

    Ent& rage_spider = state.ents.ents[ent_idx];
    StepAggroSpider(rage_spider, state, kRageSpiderAggroDistance, kRageSpiderHopSpeedX, 2, 5);
}

void StepEntLogicAsGiantSpider(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    (void)dt;

    Ent& giant_spider = state.ents.ents[ent_idx];
    StepAggroSpider(giant_spider, state, kGiantSpiderAggroDistance, kGiantSpiderHopSpeedX, 3, 6);
}

extern const EntSpec kSpiderSpec{
    .type_ = EntType::Spider,
    .size = EntSpecSize(16.0F, 16.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .affected_by_cobweb = false,
    .can_only_be_picked_up_if_dead_or_stunned = true,
    .impassable = false,
    .hurt_on_contact = true,
    .vanish_on_death = true,
    .can_be_stunned = true,
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .counter_a = EntSpecCounter(static_cast<float>(kPassiveSpiderCooldownMinFrames)),
    .damage_vuln = DamageVuln::Vulnerable,
    .damage_anim = aframe_ids::BloodBall,
    .collide_sound = audio_asset_ids::Thud,
    .step_logic = StepEntLogicAsSpider,
    .alignment = Alignment::Enemy,
    .aframe_animator = AFrameAnimator::New(aframe_ids::Spider),
};

extern const EntSpec kRageSpiderSpec{
    .type_ = EntType::RageSpider,
    .size = EntSpecSize(16.0F, 16.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .affected_by_cobweb = false,
    .can_only_be_picked_up_if_dead_or_stunned = true,
    .impassable = false,
    .hurt_on_contact = true,
    .vanish_on_death = true,
    .can_be_stunned = true,
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .counter_a = EntSpecCounter(static_cast<float>(kAggroSpiderCooldownMinFrames)),
    .damage_vuln = DamageVuln::Vulnerable,
    .damage_anim = aframe_ids::BloodBall,
    .collide_sound = audio_asset_ids::Thud,
    .step_logic = StepEntLogicAsRageSpider,
    .alignment = Alignment::Enemy,
    .aframe_animator = AFrameAnimator::New(aframe_ids::RageSpider),
};

extern const EntSpec kGiantSpiderSpec{
    .type_ = EntType::GiantSpider,
    .size = EntSpecSize(32.0F, 32.0F),
    .health = 10,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = false,
    .impassable = false,
    .hurt_on_contact = true,
    .vanish_on_death = true,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .counter_a = EntSpecCounter(static_cast<float>(kAggroSpiderCooldownMinFrames)),
    .damage_vuln = DamageVuln::Vulnerable,
    .damage_anim = aframe_ids::BloodBall,
    .collide_sound = audio_asset_ids::Thud,
    .on_death = OnDeathAsGiantSpider,
    .step_logic = StepEntLogicAsGiantSpider,
    .alignment = Alignment::Enemy,
    .aframe_animator = AFrameAnimator::New(aframe_ids::GiantSpider),
};

} // namespace splonks::ents::spider

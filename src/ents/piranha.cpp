#include "ents/piranha.hpp"

#include "audio.hpp"
#include "ent/spec.hpp"
#include "ents/common/common.hpp"
#include "aframe_animator.hpp"
#include "aframe_id.hpp"
#include "math_types.hpp"
#include "player_queries.hpp"
#include "state.hpp"
#include "water.hpp"
#include "world_query.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

namespace splonks::ents::piranha {

namespace {

constexpr float kPiranhaSwimAcceleration = 0.10F;
constexpr float kPiranhaChaseAcceleration = 0.16F;
constexpr float kPiranhaMaxSwimSpeed = 1.70F;
constexpr float kPiranhaWaterDamping = 0.98F;
constexpr float kPiranhaSurfaceDiveSpeed = 0.55F;
constexpr float kPiranhaTargetDistance = 96.0F;
constexpr float kPiranhaTargetDistanceSq = kPiranhaTargetDistance * kPiranhaTargetDistance;
constexpr float kPiranhaBiteDistance = 12.0F;
constexpr float kPiranhaBiteDistanceSq = kPiranhaBiteDistance * kPiranhaBiteDistance;

bool IsPiranhaInWater(const Ent& piranha, const State& state) {
    const Vec2 center = piranha.GetCenter();
    const float cutoff = state.settings.fluid.render_cutoff_amount;
    return IsWaterAtWorldPos(state.stage, center, cutoff) ||
           IsWaterAtWorldPos(state.stage, center + Vec2::New(0.0F, piranha.size.y * 0.35F), cutoff);
}

std::optional<Vec2> FindPiranhaTarget(const Ent& piranha, const State& state) {
    const Ent* const player = FindNearestPlayer(state, piranha.GetCenter());
    if (player == nullptr) {
        return std::nullopt;
    }

    const Vec2 delta = GetNearestWorldDelta(state.stage, piranha.GetCenter(), player->GetCenter());
    if (LengthSquared(delta) > kPiranhaTargetDistanceSq) {
        return std::nullopt;
    }
    return piranha.GetCenter() + delta;
}

void PatrolWater(Ent& piranha) {
    const float target_x = piranha.facing == Side::Left ? -kPiranhaMaxSwimSpeed : kPiranhaMaxSwimSpeed;
    piranha.acc.x += std::clamp(target_x - piranha.vel.x, -kPiranhaSwimAcceleration, kPiranhaSwimAcceleration);
    piranha.acc.y += std::clamp(-piranha.vel.y, -kPiranhaSwimAcceleration, kPiranhaSwimAcceleration);
}

void ChaseTarget(Ent& piranha, const Vec2& target, const Stage& stage) {
    const Vec2 delta = GetNearestWorldDelta(stage, piranha.GetCenter(), target);
    const Vec2 direction = NormalizeOrZeroDeterministic(delta);
    piranha.acc += direction * kPiranhaChaseAcceleration;
    if (delta.x < 0.0F) {
        piranha.facing = Side::Left;
    } else if (delta.x > 0.0F) {
        piranha.facing = Side::Right;
    }
}

struct SwimProbeResult {
    bool center = false;
    bool bottom = false;
};

SwimProbeResult QuerySwimProbes(const Ent& piranha, const State& state) {
    const AABB aabb = piranha.GetAABB();
    const Vec2 center = piranha.GetCenter();
    const float cutoff = state.settings.fluid.render_cutoff_amount;
    return SwimProbeResult{
        .center = IsWaterAtWorldPos(state.stage, center, cutoff),
        .bottom = IsWaterAtWorldPos(state.stage, Vec2::New(center.x, aabb.br.y), cutoff),
    };
}

} // namespace

void StepEntLogicAsPiranha(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    (void)dt;
    if (ent_idx >= state.ents.ents.size()) {
        return;
    }

    Ent& piranha = state.ents.ents[ent_idx];
    if (piranha.condition != EntCondition::Normal) {
        return;
    }
    if (!IsPiranhaInWater(piranha, state)) {
        TrySetAnim(piranha, EntDisplayState::Falling);
        return;
    }

    bool biting = false;
    if (const std::optional<Vec2> target = FindPiranhaTarget(piranha, state)) {
        biting = LengthSquared(GetNearestWorldDelta(state.stage, piranha.GetCenter(), *target)) <=
                 kPiranhaBiteDistanceSq;
        ChaseTarget(piranha, *target, state.stage);
    } else {
        PatrolWater(piranha);
    }
    SetMovementFlag(piranha, EntMovementFlag::Walking, true);
    if (biting) {
        SetAnim(piranha, aframe_ids::PiranhaBite);
    } else {
        TrySetAnim(piranha, EntDisplayState::Walk);
    }
}

void StepEntPhysicsAsPiranha(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    if (ent_idx >= state.ents.ents.size()) {
        return;
    }

    Ent& piranha = state.ents.ents[ent_idx];
    if (piranha.condition != EntCondition::Normal || !IsPiranhaInWater(piranha, state)) {
        common::StepStandardPhysics(ent_idx, state, graphics, audio, dt);
        return;
    }

    const Vec2 old_pos = piranha.pos;
    common::PrePartialEulerStep(ent_idx, state, dt);
    piranha.vel = piranha.vel * kPiranhaWaterDamping;
    piranha.vel.x = std::clamp(piranha.vel.x, -kPiranhaMaxSwimSpeed, kPiranhaMaxSwimSpeed);
    piranha.vel.y = std::clamp(piranha.vel.y, -kPiranhaMaxSwimSpeed, kPiranhaMaxSwimSpeed);

    const SwimProbeResult swim_probe = QuerySwimProbes(piranha, state);
    if (!swim_probe.center || !swim_probe.bottom) {
        if (swim_probe.bottom) {
            piranha.vel.y = std::max(piranha.vel.y, kPiranhaSurfaceDiveSpeed);
            piranha.vel.x *= 0.75F;
            common::DoEntCollisions(ent_idx, state, graphics, audio);
            common::PostPartialEulerStep(ent_idx, state, dt);
            return;
        }
        piranha.pos = old_pos;
        piranha.vel = Vec2::New(-piranha.vel.x * 0.5F, -piranha.vel.y * 0.5F);
        piranha.facing = piranha.facing == Side::Left ? Side::Right : Side::Left;
    }

    common::DoEntCollisions(ent_idx, state, graphics, audio);
    common::PostPartialEulerStep(ent_idx, state, dt);
}

extern const EntSpec kPiranhaSpec{
    .type_ = EntType::Piranha,
    .size = Vec2::New(8.0F, 8.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .can_only_be_picked_up_if_dead_or_stunned = true,
    .impassable = false,
    .hurt_on_contact = true,
    .can_be_stomped = false,
    .can_be_stunned = true,
    .draw_layer = DrawLayer::Middle,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::Vulnerable,
    .damage_anim = aframe_ids::BloodBall,
    .damage_sound = audio_asset_ids::CavemanHurt,
    .collide_sound = audio_asset_ids::Thud,
    .step_logic = StepEntLogicAsPiranha,
    .step_physics = StepEntPhysicsAsPiranha,
    .alignment = Alignment::Enemy,
    .aframe_animator = AFrameAnimator::New(aframe_ids::Piranha),
};

} // namespace splonks::ents::piranha

#include "entities/piranha.hpp"

#include "audio.hpp"
#include "entity/archetype.hpp"
#include "entities/common/common.hpp"
#include "frame_data_animator.hpp"
#include "frame_data_id.hpp"
#include "math_types.hpp"
#include "player_queries.hpp"
#include "state.hpp"
#include "water.hpp"
#include "world_query.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

namespace splonks::entities::piranha {

namespace {

constexpr float kPiranhaSwimAcceleration = 0.10F;
constexpr float kPiranhaChaseAcceleration = 0.16F;
constexpr float kPiranhaMaxSwimSpeed = 1.70F;
constexpr float kPiranhaWaterDamping = 0.98F;
constexpr float kPiranhaSurfaceDiveSpeed = 0.55F;
constexpr float kPiranhaTargetDistance = 96.0F;
constexpr float kPiranhaBiteDistance = 12.0F;

bool IsPiranhaInWater(const Entity& piranha, const State& state) {
    const Vec2 center = piranha.GetCenter();
    const float cutoff = state.settings.fluid.render_cutoff_amount;
    return IsWaterAtWorldPos(state.stage, center, cutoff) ||
           IsWaterAtWorldPos(state.stage, center + Vec2::New(0.0F, piranha.size.y * 0.35F), cutoff);
}

std::optional<Vec2> FindPiranhaTarget(const Entity& piranha, const State& state) {
    const Entity* const player = FindNearestPlayer(state, piranha.GetCenter());
    if (player == nullptr) {
        return std::nullopt;
    }

    const Vec2 delta = GetNearestWorldDelta(state.stage, piranha.GetCenter(), player->GetCenter());
    if (Length(delta) > kPiranhaTargetDistance) {
        return std::nullopt;
    }
    return piranha.GetCenter() + delta;
}

void PatrolWater(Entity& piranha) {
    const float target_x = piranha.facing == LeftOrRight::Left ? -kPiranhaMaxSwimSpeed : kPiranhaMaxSwimSpeed;
    piranha.acc.x += std::clamp(target_x - piranha.vel.x, -kPiranhaSwimAcceleration, kPiranhaSwimAcceleration);
    piranha.acc.y += std::clamp(-piranha.vel.y, -kPiranhaSwimAcceleration, kPiranhaSwimAcceleration);
}

void ChaseTarget(Entity& piranha, const Vec2& target, const Stage& stage) {
    const Vec2 delta = GetNearestWorldDelta(stage, piranha.GetCenter(), target);
    const Vec2 direction = NormalizeOrZero(delta);
    piranha.acc += direction * kPiranhaChaseAcceleration;
    if (delta.x < 0.0F) {
        piranha.facing = LeftOrRight::Left;
    } else if (delta.x > 0.0F) {
        piranha.facing = LeftOrRight::Right;
    }
}

struct SwimProbeResult {
    bool center = false;
    bool bottom = false;
};

SwimProbeResult QuerySwimProbes(const Entity& piranha, const State& state) {
    const AABB aabb = piranha.GetAABB();
    const Vec2 center = piranha.GetCenter();
    const float cutoff = state.settings.fluid.render_cutoff_amount;
    return SwimProbeResult{
        .center = IsWaterAtWorldPos(state.stage, center, cutoff),
        .bottom = IsWaterAtWorldPos(state.stage, Vec2::New(center.x, aabb.br.y), cutoff),
    };
}

} // namespace

void StepEntityLogicAsPiranha(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    (void)dt;
    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    Entity& piranha = state.entity_manager.entities[entity_idx];
    if (piranha.condition != EntityCondition::Normal) {
        return;
    }
    if (!IsPiranhaInWater(piranha, state)) {
        TrySetAnimation(piranha, EntityDisplayState::Falling);
        return;
    }

    bool biting = false;
    if (const std::optional<Vec2> target = FindPiranhaTarget(piranha, state)) {
        biting = Length(GetNearestWorldDelta(state.stage, piranha.GetCenter(), *target)) <= kPiranhaBiteDistance;
        ChaseTarget(piranha, *target, state.stage);
    } else {
        PatrolWater(piranha);
    }
    SetMovementFlag(piranha, EntityMovementFlag::Walking, true);
    if (biting) {
        SetAnimation(piranha, frame_data_ids::PiranhaBite);
    } else {
        TrySetAnimation(piranha, EntityDisplayState::Walk);
    }
}

void StepEntityPhysicsAsPiranha(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    Entity& piranha = state.entity_manager.entities[entity_idx];
    if (piranha.condition != EntityCondition::Normal || !IsPiranhaInWater(piranha, state)) {
        common::StepStandardPhysics(entity_idx, state, graphics, audio, dt);
        return;
    }

    const Vec2 old_pos = piranha.pos;
    common::PrePartialEulerStep(entity_idx, state, dt);
    piranha.vel = piranha.vel * kPiranhaWaterDamping;
    piranha.vel.x = std::clamp(piranha.vel.x, -kPiranhaMaxSwimSpeed, kPiranhaMaxSwimSpeed);
    piranha.vel.y = std::clamp(piranha.vel.y, -kPiranhaMaxSwimSpeed, kPiranhaMaxSwimSpeed);

    const SwimProbeResult swim_probe = QuerySwimProbes(piranha, state);
    if (!swim_probe.center || !swim_probe.bottom) {
        if (swim_probe.bottom) {
            piranha.vel.y = std::max(piranha.vel.y, kPiranhaSurfaceDiveSpeed);
            piranha.vel.x *= 0.75F;
            common::DoEntityCollisions(entity_idx, state, graphics, audio);
            common::PostPartialEulerStep(entity_idx, state, dt);
            return;
        }
        piranha.pos = old_pos;
        piranha.vel = Vec2::New(-piranha.vel.x * 0.5F, -piranha.vel.y * 0.5F);
        piranha.facing = piranha.facing == LeftOrRight::Left ? LeftOrRight::Right : LeftOrRight::Left;
    }

    common::DoEntityCollisions(entity_idx, state, graphics, audio);
    common::PostPartialEulerStep(entity_idx, state, dt);
}

extern const EntityArchetype kPiranhaArchetype{
    .type_ = EntityType::Piranha,
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
    .facing = LeftOrRight::Left,
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Vulnerable,
    .damage_animation = frame_data_ids::BloodBall,
    .damage_sound = audio_asset_ids::CavemanHurt,
    .collide_sound = audio_asset_ids::Thud,
    .step_logic = StepEntityLogicAsPiranha,
    .step_physics = StepEntityPhysicsAsPiranha,
    .alignment = Alignment::Enemy,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::Piranha),
};

} // namespace splonks::entities::piranha

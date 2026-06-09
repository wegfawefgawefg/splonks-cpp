#include "ents/arrow_trap.hpp"

#include "audio.hpp"
#include "effects.hpp"
#include "ent/spec.hpp"
#include "ents/common/common.hpp"
#include "aframe_id.hpp"
#include "state.hpp"
#include "utils.hpp"
#include "world_ops.hpp"
#include "world_query.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace splonks::ents::arrow_trap {

namespace {

constexpr int kArrowTrapMaxSensorDistance = 96;
constexpr int kArrowTrapMaxSensorTileSteps = kArrowTrapMaxSensorDistance / static_cast<int>(kTileSize);
constexpr int kArrowTrapSensorHalfHeight = 3;
constexpr float kArrowTrapMovingEntSpeed = 0.05F;
constexpr float kArrowTrapMovingEntSpeedSq =
    kArrowTrapMovingEntSpeed * kArrowTrapMovingEntSpeed;
constexpr float kArrowTrapArrowSpeed = 8.0F;
constexpr float kArrowGravity = 0.10F;
constexpr float kArrowRotationVelocityEpsilon = 0.01F;
constexpr float kArrowRotationVelocityEpsilonSq =
    kArrowRotationVelocityEpsilon * kArrowRotationVelocityEpsilon;
constexpr float kArrowImpactVelocityScale = 0.18F;
constexpr std::uint32_t kArrowDamage = 2;
constexpr std::int64_t kAngleScale = 4096;
constexpr std::int64_t kFortyFiveDegreesRaw = 45 * kAngleScale;
constexpr std::int64_t kNinetyDegreesRaw = 90 * kAngleScale;
constexpr std::int64_t kAtanCurveDegreesRaw = 16 * kAngleScale;

bool HasFired(const Ent& trap) {
    return trap.counter_a > sim::Scalar::zero();
}

int DirectionForTrap(const Ent& trap) {
    return trap.facing == Side::Left ? -1 : 1;
}

sim::FxVec2 GetSensorStart(const Ent& trap) {
    const int direction = DirectionForTrap(trap);
    return trap.GetCenter() + sim::PixelVec2(direction * 9, 0);
}

bool ShouldTriggerOnEnt(const Ent& ent) {
    return gfxp::length_sq(ent.vel) >
           ToFxScalar(kArrowTrapMovingEntSpeedSq);
}

sim::FxVec2 ArrowLaunchVelocity(int direction) {
    return sim::FxVec2{
        sim::Scalar::from_int(direction) * ToFxScalar(kArrowTrapArrowSpeed),
        sim::Scalar::zero()
    };
}

sim::FxVec2 ArrowGravityAcceleration() {
    return sim::FxVec2{sim::Scalar::zero(), ToFxScalar(kArrowGravity)};
}

void SnapArrowPositionToPixels(Ent& arrow) {
    arrow.pos = sim::PixelVec2(arrow.pos.x.floor_int(),
                               arrow.pos.y.floor_int());
}

IVec2 ToStoredArrowOffsetPoint(sim::FxVec2 offset) {
    return sim::ToPixelIVec2Round(offset);
}

sim::FxVec2 FromStoredArrowOffsetPoint(const IVec2& point) {
    return sim::FxVec2::from_pixels(point.x, point.y);
}

std::int32_t ClampAngleRaw(std::int64_t raw) {
    return static_cast<std::int32_t>(
        std::clamp(raw,
                   static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min()),
                   static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max()))
    );
}

std::int64_t AbsInt64(std::int64_t value) {
    return value < 0 ? -value : value;
}

std::int64_t AtanUnitRatioDegreesRaw(std::int64_t ratio_raw) {
    ratio_raw = std::clamp(ratio_raw, std::int64_t{0}, kAngleScale);
    const std::int64_t linear = DivRoundNearest(kFortyFiveDegreesRaw * ratio_raw, kAngleScale);
    const std::int64_t curve_numerator =
        kAtanCurveDegreesRaw * ratio_raw * (kAngleScale - ratio_raw);
    const std::int64_t curve = DivRoundNearest(curve_numerator, kAngleScale * kAngleScale);
    return linear + curve;
}

sim::Scalar ArrowRotationFromVelocity(sim::FxVec2 velocity, Side facing) {
    const std::int64_t x_raw = std::max<std::int64_t>(
        1,
        AbsInt64(static_cast<std::int64_t>(
            (velocity.x.abs() *
             sim::Scalar::from_int(static_cast<int>(kAngleScale))).round_int()))
    );
    const std::int64_t y_raw = static_cast<std::int64_t>(
        (velocity.y *
         sim::Scalar::from_int(static_cast<int>(kAngleScale))).round_int());
    const std::int64_t abs_y_raw = AbsInt64(y_raw);
    std::int64_t angle_raw = 0;
    if (abs_y_raw <= x_raw) {
        angle_raw =
            AtanUnitRatioDegreesRaw(DivRoundNearest(abs_y_raw * kAngleScale, x_raw));
    } else {
        angle_raw = kNinetyDegreesRaw -
                    AtanUnitRatioDegreesRaw(DivRoundNearest(x_raw * kAngleScale, abs_y_raw));
    }
    if (y_raw < 0) {
        angle_raw = -angle_raw;
    }
    if (facing == Side::Left) {
        angle_raw = -angle_raw;
    }
    return sim::Scalar::from_raw(ClampAngleRaw(angle_raw));
}

int GetOpenSensorCacheMarker(const Stage& stage) {
    return static_cast<int>(stage.tile_change_generation + 1U);
}

int ComputeOpenSensorDistance(const Ent& trap, const State& state) {
    const int direction = DirectionForTrap(trap);
    const sim::FxVec2 start = GetSensorStart(trap);
    const IVec2 origin_tile = state.stage.GetTileCoordAtWc(IVec2::New(
        start.x.trunc_int(),
        start.y.trunc_int()
    ));
    const TileStepRaycastResult ray = RaycastTileSteps(
        state.stage,
        origin_tile,
        IVec2::New(direction, 0),
        kArrowTrapMaxSensorTileSteps
    );
    return std::clamp(
        ray.open_steps * static_cast<int>(kTileSize),
        0,
        kArrowTrapMaxSensorDistance
    );
}

int GetCachedOpenSensorDistance(Ent& trap, const State& state) {
    const int cache_marker = GetOpenSensorCacheMarker(state.stage);
    if (trap.point_a.x != cache_marker) {
        trap.point_a.x = cache_marker;
        trap.point_a.y = ComputeOpenSensorDistance(trap, state);
    }
    return trap.point_a.y;
}

sim::FxAABB GetOpenSensorAabb(Ent& trap, const State& state) {
    const int direction = DirectionForTrap(trap);
    const sim::FxVec2 start = GetSensorStart(trap);
    const int open_distance = GetCachedOpenSensorDistance(trap, state);
    const sim::Scalar end_x = start.x + sim::Scalar::from_int(direction * open_distance);
    return sim::FxAABB::from_corners(
        sim::FxVec2{
            std::min(start.x, end_x),
            start.y - sim::Scalar::from_int(kArrowTrapSensorHalfHeight),
        },
        sim::FxVec2{
            std::max(start.x, end_x),
            start.y + sim::Scalar::from_int(kArrowTrapSensorHalfHeight),
        }
    );
}

void AddArrowTrapDebugAnnotations(Ent& trap, State& state) {
    if (!state.debug_overlay.show_debug_annotations) {
        return;
    }

    const FAABB render_sensor_aabb = ToFAABB(GetOpenSensorAabb(trap, state));
    state.AddDebugRectAnnotation(DebugRectAnnotation{
        .area = render_sensor_aabb,
        .color = DebugAnnotationColor{255, 192, 0, 255},
    });
    state.AddDebugLabelAnnotation(DebugLabelAnnotation{
        .world_pos = render_sensor_aabb.tl + FVec2::New(2.0F, -6.0F),
        .text = "arrow sensor",
        .color = DebugAnnotationColor{255, 192, 0, 255},
    });
}

bool SensorTouchesMovingEnt(
    Ent& trap,
    const State& state,
    const Graphics& graphics
) {
    const sim::FxAABB sensor_aabb = GetOpenSensorAabb(trap, state);
    if (sensor_aabb.br.x <= sensor_aabb.tl.x) {
        return false;
    }

    const std::vector<VID> hits = QueryEntsInAabb(state, sensor_aabb, trap.vid);
    for (const VID& vid : hits) {
        const Ent* const ent = state.ents.GetEnt(vid);
        if (ent == nullptr || !ent->active || !ent->can_be_hit) {
            continue;
        }
        if (!ShouldTriggerOnEnt(*ent)) {
            continue;
        }
        if (!WorldAabbsIntersect(
                state.stage,
                sensor_aabb,
                ents::common::GetContactAabbForEnt(*ent, graphics)
            )) {
            continue;
        }
        return true;
    }

    return false;
}

Ent* SpawnArrow(State& state, sim::FxVec2 center, int direction, const VID& trap_vid) {
    return world_ops::SpawnEnt(state, EntType::Arrow, [&](Ent& arrow) {
        arrow.SetCenter(center);
        arrow.vel = ArrowLaunchVelocity(direction);
        arrow.acc = sim::FxVec2::zero();
        arrow.facing = direction < 0 ? Side::Left : Side::Right;
        arrow.rotation = sim::Scalar::zero();
        arrow.thrown_by = trap_vid;
        arrow.thrown_immunity_timer = ents::common::kThrownByImmunityDuration;
        arrow.proj_contact_damage_type = DamageType::Attack;
        arrow.proj_contact_damage_amount = kArrowDamage;
        arrow.proj_contact_timer = ents::common::kProjContactDuration;
        arrow.can_apply_proj_contact = false;
        (void)AddEffect(arrow, EffectId::NoGravityUntilContact);
    });
}

Ent* SpawnLooseArrow(State& state, sim::FxVec2 center) {
    return world_ops::SpawnEnt(state, EntType::Arrow, [&](Ent& arrow) {
        arrow.SetCenter(center);
        SnapArrowPositionToPixels(arrow);
        arrow.vel = sim::FxVec2::zero();
        arrow.acc = sim::FxVec2::zero();
        arrow.proj_contact_timer = 0;
        arrow.proj_contact_damage_amount = kArrowDamage;
        arrow.can_apply_proj_contact = false;
        arrow.thrown_by.reset();
    });
}

void FireTrap(std::size_t ent_idx, State& state, Audio& audio) {
    (void)audio;
    Ent& trap = state.ents.ents[ent_idx];
    if (HasFired(trap)) {
        return;
    }

    const int direction = DirectionForTrap(trap);
    const sim::FxVec2 arrow_center = trap.GetCenter() + sim::FxVec2::from_pixels(direction * 10, -4);
    Ent* const arrow = SpawnArrow(state, arrow_center, direction, trap.vid);
    if (arrow == nullptr) {
        return;
    }
    trap.counter_a = sim::Scalar::from_int(1);
    (void)PlayWorldSoundEmitter(state, ToFVec2(arrow_center), audio_asset_ids::Throw);
}

void StepEntLogicAsArrowTrap(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)dt;
    if (ent_idx >= state.ents.ents.size()) {
        return;
    }

    Ent& trap = state.ents.ents[ent_idx];
    if (!trap.active || HasFired(trap)) {
        return;
    }

    AddArrowTrapDebugAnnotations(trap, state);

    if (!SensorTouchesMovingEnt(trap, state, graphics)) {
        return;
    }

    FireTrap(ent_idx, state, audio);
}

void StepEntLogicAsArrow(
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

    Ent& arrow = state.ents.ents[ent_idx];
    if (arrow.held_by_vid.has_value()) {
        arrow.ent_a.reset();
        arrow.rotation = sim::Scalar::zero();
        SnapArrowPositionToPixels(arrow);
        return;
    }

    if (arrow.ent_a.has_value()) {
        const Ent* const stuck_to = state.ents.GetEnt(*arrow.ent_a);
        if (stuck_to == nullptr || !stuck_to->active) {
            arrow.ent_a.reset();
            arrow.has_physics = true;
            arrow.can_collide = true;
            arrow.vel = sim::FxVec2::zero();
            arrow.acc = ArrowGravityAcceleration();
            arrow.proj_contact_timer = 0;
            arrow.can_apply_proj_contact = false;
            return;
        }

        arrow.has_physics = false;
        arrow.can_collide = false;
        arrow.vel = sim::FxVec2::zero();
        arrow.acc = sim::FxVec2::zero();
        arrow.SetCenter(stuck_to->GetCenter() + FromStoredArrowOffsetPoint(arrow.point_a));
        SnapArrowPositionToPixels(arrow);
        return;
    }

    if (!arrow.has_physics) {
        SnapArrowPositionToPixels(arrow);
        return;
    }

    if (gfxp::length_sq(arrow.vel) > ToFxScalar(kArrowRotationVelocityEpsilonSq)) {
        if (arrow.proj_contact_timer > 0) {
            arrow.proj_contact_damage_amount = kArrowDamage;
        }
        if (arrow.vel.x.abs() > ToFxScalar(kArrowRotationVelocityEpsilon)) {
            arrow.facing = arrow.vel.x < sim::Scalar::zero() ? Side::Left : Side::Right;
        }
        arrow.rotation = ArrowRotationFromVelocity(arrow.vel, arrow.facing);
    }
    const float gravity_scale =
        GetModifiedEffectValue(arrow, EffectModifierTarget::GravityScale, 1.0F);
    arrow.acc.y += ToFxScalar(kArrowGravity * gravity_scale);
}

bool CanArrowHitEnt(const Ent& arrow, const Ent& other) {
    if (!arrow.active || !other.active || arrow.proj_contact_timer == 0) {
        return false;
    }
    if (other.type_ == EntType::Arrow) {
        return false;
    }
    if (arrow.held_by_vid.has_value()) {
        return false;
    }
    if (!other.can_be_hit || !other.can_receive_proj_contact || !other.can_collide) {
        return false;
    }
    if (arrow.thrown_by.has_value() && other.vid == *arrow.thrown_by) {
        return false;
    }
    return true;
}

Ent* GetHeldBow(Ent& collector, State& state) {
    if (!collector.holding_vid.has_value()) {
        return nullptr;
    }

    Ent* const held = state.ents.GetEntMut(*collector.holding_vid);
    if (held == nullptr || !held->active || held->type_ != EntType::Bow) {
        return nullptr;
    }
    return held;
}

bool TryCollectLooseArrowIntoHeldBow(
    std::size_t arrow_idx,
    std::size_t collector_idx,
    State& state,
    const Graphics* graphics,
    Audio* audio
) {
    if (graphics == nullptr || audio == nullptr ||
        arrow_idx >= state.ents.ents.size() ||
        collector_idx >= state.ents.ents.size()) {
        return false;
    }

    Ent& arrow = state.ents.ents[arrow_idx];
    Ent& collector = state.ents.ents[collector_idx];
    if (!arrow.active || arrow.held_by_vid.has_value() || arrow.proj_contact_timer > 0 ||
        arrow.buyable.active || !collector.can_collect_pickups) {
        return false;
    }

    Ent* const bow = GetHeldBow(collector, state);
    if (bow == nullptr) {
        return false;
    }

    bow->counter_b += sim::Scalar::from_int(1);
    if (!bow->ent_a.has_value()) {
        SetAnim(
            *bow,
            bow->counter_b > sim::Scalar::zero() ? aframe_ids::BowLooseLoaded
                                                 : aframe_ids::BowLooseEmpty
        );
    }
    (void)PlayEntCenterSoundEmitter(state, *bow, audio_asset_ids::Equip);
    common::DeactivateCollectedPickup(arrow_idx, state, *graphics);
    return true;
}

void StickArrowToEnt(Ent& arrow, Ent& other, State& state) {
    const sim::FxVec2 other_center = other.GetCenter();
    const sim::FxVec2 arrow_center = GetNearestWorldPoint(state.stage, other_center, arrow.GetCenter());
    const IVec2 stored_offset = ToStoredArrowOffsetPoint(arrow_center - other_center);

    arrow.SetCenter(other_center + FromStoredArrowOffsetPoint(stored_offset));
    SnapArrowPositionToPixels(arrow);
    arrow.ent_a = other.vid;
    arrow.point_a = stored_offset;
    arrow.has_physics = false;
    arrow.can_collide = false;
    arrow.proj_contact_timer = 0;
    arrow.can_apply_proj_contact = false;
    arrow.thrown_by.reset();
    arrow.thrown_immunity_timer = 0;
    arrow.vel = sim::FxVec2::zero();
    arrow.acc = sim::FxVec2::zero();
}

ents::common::ContactResult OnEntContactAsArrow(
    std::size_t ent_idx,
    std::size_t other_ent_idx,
    const ents::common::ContactContext& context,
    State& state,
    const Graphics* graphics,
    Audio* audio
) {
    (void)graphics;
    (void)audio;
    if (TryCollectLooseArrowIntoHeldBow(ent_idx, other_ent_idx, state, graphics, audio)) {
        return {};
    }

    if (ent_idx >= state.ents.ents.size() ||
        other_ent_idx >= state.ents.ents.size()) {
        return {};
    }

    Ent& arrow = state.ents.ents[ent_idx];
    Ent& other_ent = state.ents.ents[other_ent_idx];

    const bool swept_contact = context.phase == ents::common::ContactPhase::SweptEntered;
    const bool blocked_impassable_contact =
        context.phase == ents::common::ContactPhase::AttemptedBlocked &&
        context.has_impact &&
        other_ent.impassable;
    if ((!swept_contact && !blocked_impassable_contact) || audio == nullptr) {
        return {};
    }

    if (!CanArrowHitEnt(arrow, other_ent)) {
        return {};
    }

    const sim::FxVec2 impact_velocity = arrow.vel;
    if (arrow.collide_sound.has_value()) {
        (void)PlayWorldSoundEmitter(state, ToFVec2(arrow.GetCenter()), *arrow.collide_sound);
    }
    const ents::common::DamageResult damage_result = ents::common::TryHitEnt(
        other_ent_idx,
        state,
        *audio,
        arrow.proj_contact_damage_type,
        arrow.proj_contact_damage_amount,
        ents::common::HitOptions{
            .source_vid = arrow.vid,
            .knockback = ents::common::KnockbackSpec{
                .velocity = arrow.vel * ToFxScalar(kArrowImpactVelocityScale),
                .clear_velocity = false,
                .clear_acceleration = true,
                .thrown_by = arrow.thrown_by,
                .thrown_immunity_timer = ents::common::kThrownByImmunityDuration,
                .proj_contact_damage_type = DamageType::Attack,
                .proj_contact_damage_amount = 1,
                .proj_contact_duration = ents::common::kProjContactDuration,
            },
        }
    );
    (void)damage_result;
    arrow.vel = impact_velocity;
    Ent& updated_other_ent = state.ents.ents[other_ent_idx];
    if (updated_other_ent.active) {
        StickArrowToEnt(arrow, updated_other_ent, state);
    } else {
        (void)world_ops::DeactivateEnt(state, arrow.vid);
    }

    return ents::common::ContactResult{.stop_sweep = true};
}

ents::common::ContactResult OnTileContactAsArrow(
    std::size_t ent_idx,
    const ents::common::ContactContext& context,
    State& state
) {
    if (context.phase != ents::common::ContactPhase::AttemptedBlocked ||
        ent_idx >= state.ents.ents.size()) {
        return {};
    }

    Ent& arrow = state.ents.ents[ent_idx];
    if (!arrow.active) {
        return {};
    }

    arrow.vel = sim::FxVec2::zero();
    arrow.acc = sim::FxVec2::zero();
    arrow.proj_contact_timer = 0;
    arrow.proj_contact_damage_amount = 0;
    arrow.can_apply_proj_contact = false;
    arrow.thrown_by.reset();
    arrow.has_physics = false;
    SnapArrowPositionToPixels(arrow);
    return ents::common::ContactResult{.stop_sweep = true};
}

EntDamageEffectResult OnDamageAsArrow(
    std::size_t ent_idx,
    State& state,
    Audio& audio,
    DamageType damage_type,
    std::uint32_t amount,
    bool damage_applied
) {
    (void)audio;
    (void)amount;
    (void)damage_applied;
    if (damage_type != DamageType::Explosion ||
        ent_idx >= state.ents.ents.size()) {
        return EntDamageEffectResult::None;
    }

    Ent& arrow = state.ents.ents[ent_idx];
    arrow.has_physics = true;
    arrow.can_collide = true;
    arrow.can_apply_proj_contact = false;
    arrow.proj_contact_damage_type = DamageType::Attack;
    arrow.proj_contact_damage_amount = kArrowDamage;
    return EntDamageEffectResult::None;
}

void OnDeathAsArrowTrap(std::size_t ent_idx, State& state, Audio& audio) {
    (void)audio;
    if (ent_idx >= state.ents.ents.size()) {
        return;
    }

    const Ent& trap = state.ents.ents[ent_idx];
    if (HasFired(trap)) {
        return;
    }
    (void)SpawnLooseArrow(state, trap.GetCenter());
}

} // namespace

extern const EntSpec kArrowTrapSpec{
    .type_ = EntType::ArrowTrap,
    .size = EntSpecSize(16.0F, 16.0F),
    .health = 1,
    .has_physics = false,
    .can_collide = true,
    .can_be_picked_up = false,
    .impassable = true,
    .can_be_hung_on = true,
    .hurt_on_contact = false,
    .vanish_on_death = true,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Middle,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::ExplosionOnly,
    .on_death = OnDeathAsArrowTrap,
    .step_logic = StepEntLogicAsArrowTrap,
    .alignment = Alignment::Enemy,
    .aframe_animator = AFrameAnimator::New(aframe_ids::ArrowTrap),
};

extern const EntSpec kArrowSpec{
    .type_ = EntType::Arrow,
    .size = EntSpecSize(8.0F, 8.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .can_be_stunned = false,
    .affected_by_ground_friction = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::CrushingOnly,
    .proj_contact_damage_amount = kArrowDamage,
    .can_apply_proj_contact = false,
    .collide_sound = audio_asset_ids::Thud,
    .on_damage = OnDamageAsArrow,
    .step_logic = StepEntLogicAsArrow,
    .on_ent_contact = OnEntContactAsArrow,
    .on_tile_contact = OnTileContactAsArrow,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator::New(aframe_ids::Arrow),
};

} // namespace splonks::ents::arrow_trap

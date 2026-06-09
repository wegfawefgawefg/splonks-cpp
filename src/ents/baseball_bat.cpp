#include "ents/baseball_bat.hpp"

#include "audio.hpp"
#include "ent/spec.hpp"
#include "ents/common/common.hpp"
#include "aframe_id.hpp"
#include "particles/ribbon_particle.hpp"
#include "sim/fxp.hpp"
#include "state.hpp"
#include "utils.hpp"
#include "world_ops.hpp"
#include "world_query.hpp"

#include <memory>
#include <vector>

namespace splonks::ents::baseball_bat {

namespace {


constexpr std::uint32_t kBatTrailLifetimeFrames = 6;
constexpr float kBatTrailMinDistance = 2.0F;
constexpr float kBatTrailMinDistanceSq = kBatTrailMinDistance * kBatTrailMinDistance;
const sim::FxVec2 kBatHoldOffset = sim::PixelVec2(5, -10);

void SpawnBatTrailSegment(State& state, const FVec2& from, const FVec2& to) {
    const FVec2 wrapped_to = GetNearestWorldPoint(state.stage, from, to);
    if (LengthSquared(wrapped_to - from) < kBatTrailMinDistanceSq) {
        return;
    }

    RibbonParticle ribbon{};
    ribbon.counter = kBatTrailLifetimeFrames;
    ribbon.spec_id = ribbon_particle_spec_ids::BaseballBatTrail;
    ribbon.alpha = 0.36F;
    ribbon.point_count = 2;
    ribbon.points[0] = from;
    ribbon.points[1] = wrapped_to;
    state.particles.Add(std::move(ribbon));
}

IVec2 ToWorldPixelTrunc(sim::FxVec2 point) {
    return IVec2::New(point.x.to_pixels_trunc(), point.y.to_pixels_trunc());
}

SwingStage GetSwingStage(const Ent& baseball_bat) {
    switch (baseball_bat.aframe_animator.current_frame) {
    case 0:
        return SwingStage::Back;
    case 1:
        return SwingStage::Above;
    default:
        return SwingStage::Swing;
    }
}

} // namespace

common::ContactResult OnEntContactAsBaseballBat(
    std::size_t bat_ent_idx,
    std::size_t other_ent_idx,
    const common::ContactContext& context,
    State& state,
    const Graphics* graphics,
    Audio* audio
) {
    (void)context;
    if (graphics == nullptr || audio == nullptr) {
        return common::ContactResult{};
    }

    const bool applied = TryApplyBatContactToEnt(
        bat_ent_idx,
        other_ent_idx,
        state,
        *graphics,
        *audio
    );
    return common::ContactResult{
        .blocks_movement = false,
        .stop_sweep = applied,
    };
}

extern const EntSpec kBaseballBatSpec{
    .type_ = EntType::BaseballBat,
    .size = EntSpecSize(12.0F, 4.0F),
    .health = 1,
    .has_physics = false,
    .can_collide = false,
    .can_be_picked_up = false,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stunned = false,
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::Immune,
    .step_logic = StepBaseballBat,
    .on_ent_contact = OnEntContactAsBaseballBat,
    .ent_contact_cooldown_duration = kBatContactCooldownFrames,
    .alignment = Alignment::Neutral,
    .aframe_animator = AFrameAnimator{
        .anim_id = aframe_ids::BaseballBatSwing,
        .current_frame = 0,
        .current_time = sim::Scalar::zero(),
        .scale = sim::Scalar::from_int(1),
        .speed = sim::Scalar::from_int(1),
        .animate = true,
        .loop = false,
        .finished = false,
    },
};

bool TryApplyBatContactToEnt(
    std::size_t bat_ent_idx,
    std::size_t other_ent_idx,
    State& state,
    const Graphics& graphics,
    Audio& audio
) {
    if (bat_ent_idx == other_ent_idx) {
        return false;
    }
    if (bat_ent_idx >= state.ents.ents.size() ||
        other_ent_idx >= state.ents.ents.size()) {
        return false;
    }

    const Ent& bat_ent = state.ents.ents[bat_ent_idx];
    if (!bat_ent.active || bat_ent.type_ != EntType::BaseballBat) {
        return false;
    }
    const Ent& other_ent_const = state.ents.ents[other_ent_idx];
    if (!other_ent_const.active || other_ent_const.impassable || !other_ent_const.can_collide) {
        return false;
    }
    if (bat_ent.held_by_vid.has_value() && other_ent_const.vid == *bat_ent.held_by_vid) {
        return false;
    }

    const sim::FxAABB bat_aabb = common::GetContactAabbForEnt(bat_ent, graphics);
    const sim::FxAABB other_aabb = GetNearestWorldAabb(
        state.stage,
        bat_aabb.center(),
        common::GetContactAabbForEnt(other_ent_const, graphics)
    );
    if (!gfxp::aabbs_intersect(bat_aabb, other_aabb)) {
        return false;
    }

    const Side bat_facing = bat_ent.facing;
    const std::optional<VID> held_by_vid = bat_ent.held_by_vid;
    const SwingStage swing_stage = GetSwingStage(bat_ent);

    if (Ent* const other_ent = state.ents.GetEntMut(other_ent_const.vid)) {
        const sim::Scalar knock_back_impulse = sim::Scalar::from_int(10);
        const sim::Scalar air_knock_back_lift = sim::Scalar::from_int(4);
        const bool should_lift_target =
            !other_ent->grounded || other_ent->vel.y > sim::Scalar::zero();
        sim::FxVec2 knock_back_vel = other_ent->vel;
        switch (swing_stage) {
        case SwingStage::Back:
            knock_back_vel = bat_facing == Side::Left
                                 ? sim::FxVec2{knock_back_impulse,
                                             should_lift_target ? -air_knock_back_lift
                                                                : sim::Scalar::zero()}
                                 : sim::FxVec2{-knock_back_impulse,
                                             should_lift_target ? -air_knock_back_lift
                                                                : sim::Scalar::zero()};
            break;
        case SwingStage::Above:
            knock_back_vel = bat_facing == Side::Left
                                 ? sim::FxVec2{-(knock_back_impulse / 2), -knock_back_impulse}
                                 : sim::FxVec2{knock_back_impulse / 2, -knock_back_impulse};
            break;
        case SwingStage::Swing:
            knock_back_vel = bat_facing == Side::Left
                                 ? sim::FxVec2{-knock_back_impulse,
                                             should_lift_target ? -air_knock_back_lift
                                                                : sim::Scalar::zero()}
                                 : sim::FxVec2{knock_back_impulse,
                                             should_lift_target ? -air_knock_back_lift
                                                                : sim::Scalar::zero()};
            break;
        }
        const common::DamageResult damage_result = common::TryHitEnt(
            other_ent->vid.id,
            state,
            audio,
            DamageType::Attack,
            1,
            common::HitOptions{
                .source_vid = bat_ent.vid,
                .knockback = common::KnockbackSpec{
                    .velocity = knock_back_vel,
                    .clear_velocity = true,
                    .clear_acceleration = true,
                    .thrown_by = held_by_vid,
                    .thrown_immunity_timer = common::kThrownByImmunityDuration,
                    .proj_contact_damage_type = DamageType::Attack,
                    .proj_contact_damage_amount = 1,
                    .proj_contact_duration = common::kProjContactDuration,
                },
            }
        );
        switch (damage_result) {
        case common::DamageResult::Died: {
            const int random_number = state.drng.RandomIntInclusive(0, 10);
            std::optional<AudioAssetId> sound_effect;
            if (random_number <= 8) {
                const int another_random_number = state.drng.RandomIntInclusive(0, 2);
                switch (another_random_number) {
                case 0:
                    sound_effect = audio_asset_ids::BaseballBatKillHit1;
                    break;
                case 1:
                    sound_effect = audio_asset_ids::BaseballBatKillHit2;
                    break;
                default:
                    sound_effect = audio_asset_ids::BaseballBatKillHit3;
                    break;
                }
            }
            if (sound_effect.has_value()) {
                (void)PlayEntCenterSoundEmitter(state, state.ents.ents[bat_ent_idx], *sound_effect);
            }
            break;
        }
        case common::DamageResult::None: {
            (void)PlayEntCenterSoundEmitter(state, state.ents.ents[bat_ent_idx], audio_asset_ids::BaseballBatMetalDink1);
            break;
        }
        case common::DamageResult::Hurt:
            (void)PlayEntCenterSoundEmitter(state, state.ents.ents[bat_ent_idx], audio_asset_ids::Thud);
            break;
        }
        return true;
    }

    return false;
}

void StepBaseballBat(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)dt;
    // delete conditions
    //  //  held by is gone // player die or seomthing, stunned, etc
    Ent& baseball_bat = state.ents.ents[ent_idx];
    const std::optional<VID> held_by_vid = baseball_bat.held_by_vid;
    if (!held_by_vid.has_value()) {
        (void)world_ops::DeactivateEnt(state, baseball_bat.vid);
        return;
    }
    if (baseball_bat.aframe_animator.IsFinished()) {
        (void)world_ops::DeactivateEnt(state, baseball_bat.vid);
        return;
    }

    sim::FxVec2 swinger_center = sim::FxVec2::zero();
    Side swinger_facing = Side::Left;
    if (held_by_vid.has_value()) {
        if (const Ent* const held_by = state.ents.GetEnt(*held_by_vid)) {
            swinger_center = held_by->GetSimCenter();
            swinger_facing = held_by->facing;
        }
    }

    baseball_bat.facing = swinger_facing;
    const sim::FxVec2 mounted_center = swinger_facing == Side::Left
                                         ? swinger_center +
                                               sim::FxVec2{-kBatHoldOffset.x, kBatHoldOffset.y}
                                         : swinger_center + kBatHoldOffset;
    baseball_bat.SetSimCenter(mounted_center);

    const sim::FxVec2 bat_emit_point =
        common::GetEmitPointForEnt(baseball_bat, graphics, baseball_bat.GetSimCenter());
    const FVec2 render_bat_emit_point = ToFVec2(bat_emit_point);
    if (baseball_bat.point_label_a != PointLabel::Target) {
        baseball_bat.point_label_a = PointLabel::Target;
        baseball_bat.point_a = ToWorldPixelTrunc(bat_emit_point);
    } else {
        SpawnBatTrailSegment(state, ToVec2(baseball_bat.point_a), render_bat_emit_point);
        const sim::FxVec2 nearest_emit_point = GetNearestWorldPoint(
            state.stage,
            sim::PixelVec2(baseball_bat.point_a.x, baseball_bat.point_a.y),
            bat_emit_point
        );
        baseball_bat.point_a = ToWorldPixelTrunc(nearest_emit_point);
    }

    state.UpdateSidForEnt(ent_idx, graphics);
    common::TryDispatchEntEntOverlapContacts(
        ent_idx,
        state,
        graphics,
        audio,
        common::ContactContext{
            .phase = common::ContactPhase::SweptEntered,
            .has_impact = false,
            .mover_vid = baseball_bat.vid,
        }
    );
}

/** generalize this to all square or rectangular ents somehow */
bool IsStuff(EntType type_) {
    switch (type_) {
    case EntType::Pot:
    case EntType::Box:
        return true;
    default:
        return false;
    }
}

} // namespace splonks::ents::baseball_bat

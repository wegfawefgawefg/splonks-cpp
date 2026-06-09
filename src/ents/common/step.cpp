#include "ents/common/common.hpp"
#include "ent/spec.hpp"

#include "tile.hpp"
#include "world_ops.hpp"
#include "world_query.hpp"

#include <algorithm>
#include <cmath>

namespace splonks::ents::common {

namespace {

constexpr float kProjSettleSpeedThreshold = 0.5F;

} // namespace

void ApplyDeactivateConditions(std::size_t ent_idx, State& state) {
    Ent& ent = state.ents.ents[ent_idx];
    const bool vanish_on_death = ent.vanish_on_death;
    if ((vanish_on_death && ent.condition == EntCondition::Dead) ||
        ent.marked_for_destruction) {
        (void)world_ops::DeactivateEnt(state, ent.vid);
    }
}

void StepStunTimer(std::size_t ent_idx, State& state) {
    Ent& ent = state.ents.ents[ent_idx];
    if (ent.contact_sound_cooldown > 0) {
        ent.contact_sound_cooldown -= 1;
    }
    if (ent.condition == EntCondition::Stunned) {
        const auto recover_from_stun = [&ent, &state]() {
            if (ent.held_by_vid.has_value()) {
                ReleaseEntFromHolder(ent, state);
            }
            const EntSpec& spec = GetEntSpec(ent.type_);
            ent.proj_contact_damage_type = spec.proj_contact_damage_type;
            ent.proj_contact_damage_amount = spec.proj_contact_damage_amount;
            ent.proj_contact_timer = 0;
            ent.condition = EntCondition::Normal;
            TrySetAnim(ent, EntDisplayState::Neutral);
        };

        if (ent.stun_timer == 0) {
            recover_from_stun();
            return;
        }

        const bool held = ent.held_by_vid.has_value();
        const bool can_advance_stun_timer =
            (ent.stun_recovers_on_ground && ent.grounded) ||
            (ent.stun_recovers_while_held && held);
        if (!can_advance_stun_timer) {
            return;
        }

        ent.stun_timer -= 1;
        if (ent.stun_timer == 0) {
            recover_from_stun();
        }
    }
}

void AccelerateHorizontallyTowardSpeed(Ent& ent, float target_speed, float max_acceleration) {
    const FxScalar delta = ToFxScalar(target_speed) - ent.vel.x;
    const FxScalar max_acc = ToFxScalar(max_acceleration);
    ent.acc.x += gfxp::clamp(delta, -max_acc, max_acc);
}

void AccelerateHorizontallyTowardSpeed(
    Ent& ent,
    const State& state,
    float target_speed,
    float max_acceleration
) {
    const float move_speed_scale = std::max(
        0.0F,
        GetModifiedEffectValue(ent, EffectModifierTarget::MoveSpeedScale, 1.0F, &state)
    );
    AccelerateHorizontallyTowardSpeed(ent, target_speed * move_speed_scale, max_acceleration);
}

void DecelerateHorizontallyToStop(Ent& ent, float max_acceleration, float snap_speed) {
    if (ent.vel.x.abs() <= ToFxScalar(snap_speed)) {
        ent.vel.x = FxScalar::zero();
        return;
    }
    AccelerateHorizontallyTowardSpeed(ent, 0.0F, max_acceleration);
}

void StepTravelSoundWalkerClimber(std::size_t ent_idx, State& state, Audio& audio) {
    (void)audio;
    constexpr float kWalkerStepVolumeScale = 0.70F;
    constexpr float kClimberStepVolumeScale = 0.90F;
    Ent& ent = state.ents.ents[ent_idx];
    ent.travel_sound_countdown -= ent.dist_traveled_this_frame;

    if (!ent.grounded && !ent.IsClimbing()) {
        return;
    }

    if (ent.travel_sound_countdown < FxScalar::zero()) {
        ent.travel_sound_countdown = ent.IsClimbing()
                                            ? FxScalar::from_int(static_cast<std::int32_t>(
                                                  kClimberTravelSoundDistInterval))
                                            : FxScalar::from_int(static_cast<std::int32_t>(
                                                  kWalkerClimberTravelSoundDistInterval));

        AudioAssetId which_step_sound;
        if (ent.IsClimbing()) {
            const FxAABB ent_aabb = ent.GetAABB();
            bool its_rope = false;
            bool its_ladder = false;
            for (const WorldTileQueryResult& tile_query : QueryTilesInAabb(state.stage, ent_aabb)) {
                if (tile_query.tile == nullptr) {
                    continue;
                }
                if (*tile_query.tile == Tile::Rope) {
                    its_rope = true;
                }
                if (*tile_query.tile == Tile::Ladder) {
                    its_ladder = true;
                }
            }
            if (its_rope) {
                which_step_sound = ent.travel_sound == TravelSound::One
                                       ? audio_asset_ids::ClimbRope1
                                       : audio_asset_ids::ClimbRope2;
            } else if (its_ladder) {
                which_step_sound = ent.travel_sound == TravelSound::One
                                       ? audio_asset_ids::ClimbMetal1
                                       : audio_asset_ids::ClimbMetal2;
            } else {
                which_step_sound = audio_asset_ids::Step1;
            }
        } else {
            which_step_sound =
                ent.travel_sound == TravelSound::One ? audio_asset_ids::Step1 : audio_asset_ids::Step2;
        }
        AudioEmitterPlayParams params;
        params.volume_scale = ent.IsClimbing() ? kClimberStepVolumeScale : kWalkerStepVolumeScale;
        (void)PlayEntSoundEmitter(state, ent, which_step_sound, params);
        ent.IncTravelSound();
    }
}

void DoThrownByStep(std::size_t ent_idx, State& state) {
    Ent& ent = state.ents.ents[ent_idx];
    const std::optional<VID> thrown_by = ent.thrown_by;
    if (thrown_by) {
        if (ent.thrown_immunity_timer > 0) {
            ent.thrown_immunity_timer -= 1;
        }
    }
    if (ent.thrown_immunity_timer == 0) {
        ent.thrown_by.reset();
    }

    if (ent.proj_contact_timer == 0) {
        return;
    }

    const bool settled_on_ground =
        ent.grounded && ent.vel.x.abs() <= ToFxScalar(kProjSettleSpeedThreshold) &&
        ent.vel.y.abs() <= ToFxScalar(kProjSettleSpeedThreshold);
    if (settled_on_ground) {
        ent.proj_contact_timer -= 1;
        if (ent.proj_contact_timer == 0) {
            const EntSpec& spec = GetEntSpec(ent.type_);
            ent.proj_contact_damage_type = spec.proj_contact_damage_type;
            ent.proj_contact_damage_amount = spec.proj_contact_damage_amount;
        }
    } else {
        ent.proj_contact_timer = kProjContactDuration;
    }
}

} // namespace splonks::ents::common

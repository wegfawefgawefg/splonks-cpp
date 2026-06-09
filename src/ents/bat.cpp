#include "ents/bat.hpp"
#include "on_damage_effects.hpp"

#include "audio.hpp"
#include "ent/spec.hpp"
#include "ents/common/common.hpp"
#include "aframe_id.hpp"
#include "state.hpp"
#include "controls.hpp"
#include "tile.hpp"
#include "world_query.hpp"

#include <algorithm>
#include <vector>

namespace splonks::ents::bat {

namespace {

sim::AABB GetAreaAbove(const Ent& bat) {
    const sim::AABB aabb = bat.GetSimAABB();
    return sim::AABB::from_corners(
        sim::FxVec2{aabb.tl.x, aabb.tl.y - sim::Scalar::from_pixels(1)},
        sim::FxVec2{aabb.br.x, aabb.tl.y}
    );
}

bool IsAtPerchOrRoof(const Ent& bat, const State& state) {
    const sim::AABB area_above = GetAreaAbove(bat);
    if (area_above.tl.y < sim::Scalar::zero()) {
        return true;
    }
    for (const WorldTileQueryResult& tile_query : QueryTilesInAabb(state.stage, area_above)) {
        if (tile_query.tile != nullptr && IsTileCollidable(*tile_query.tile)) {
            return true;
        }
    }
    return false;
}

std::optional<sim::FxVec2> FindBatTargetPosition(const Ent& bat, const State& state) {
    constexpr int kVerticalDetectDist = 8 * static_cast<int>(kTileSize);
    constexpr int kHorizontalChaseDist = 4 * static_cast<int>(kTileSize);
    const sim::Scalar vertical_detect_dist = sim::Scalar::from_pixels(kVerticalDetectDist);
    const sim::Scalar horizontal_chase_dist = sim::Scalar::from_pixels(kHorizontalChaseDist);

    std::optional<sim::FxVec2> best_target;
    sim::Scalar best_dist_sq{};
    const sim::FxVec2 bat_pos = bat.GetSimPos();
    for (const PlayerSlot& slot : state.players.slots) {
        if (!slot.connected || !slot.ent_vid.has_value()) {
            continue;
        }

        const Ent* const player = state.ents.GetEnt(*slot.ent_vid);
        if (player == nullptr || !player->active || player->condition != EntCondition::Normal) {
            continue;
        }

        const sim::FxVec2 player_delta =
            GetNearestWorldDelta(state.stage, bat_pos, player->GetSimPos());
        if (player_delta.y <= sim::Scalar::zero() ||
            player_delta.y.abs() >= vertical_detect_dist ||
            player_delta.x.abs() >= horizontal_chase_dist) {
            continue;
        }

        const sim::Scalar dist_sq = gfxp::length_sq(player_delta);
        if (!best_target.has_value() || dist_sq < best_dist_sq) {
            best_dist_sq = dist_sq;
            best_target = bat_pos + player_delta;
        }
    }
    return best_target;
}

void SnapBatToRoof(Ent& bat, const State& state) {
    if (IsAtPerchOrRoof(bat, state)) {
        return;
    }

    for (int i = 0; i < static_cast<int>(kTileSize); ++i) {
        bat.pos.y -= sim::Scalar::from_pixels(1);
        if (IsAtPerchOrRoof(bat, state)) {
            return;
        }
    }
}

void ControlEntAsBat(
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

    Ent& bat = state.ents.ents[ent_idx];
    const controls::ControlIntent control = controls::GetControlIntentForEnt(bat, state);
    const bool steering = control.left || control.right || control.up || control.down;
    if (bat.condition != EntCondition::Normal) {
        return;
    }

    if (!steering && IsAtPerchOrRoof(bat, state)) {
        bat.ai_state = EntAiState::Idle;
        SetAnim(bat, aframe_ids::HangingBat);
        bat.acc = sim::FxVec2::zero();
        bat.vel = sim::FxVec2::zero();
        return;
    }

    if (control.use_pressed) {
        (void)PlayEntSoundEmitter(state, bat, audio_asset_ids::BatSqueak);
    }
    if (bat.ai_state == EntAiState::Idle && steering) {
        (void)PlayEntSoundEmitter(state, bat, audio_asset_ids::BatSqueak);
    }

    bat.ai_state = EntAiState::Pursuing;
    SetAnim(bat, aframe_ids::FlyingBat);
    bat.acc = sim::FxVec2::zero();
    const sim::Scalar chase_speed = sim::ToSimScalar(kChaseSpeed);
    if (control.left) {
        bat.acc.x -= chase_speed;
    }
    if (control.right) {
        bat.acc.x += chase_speed;
    }
    if (control.up) {
        bat.acc.y -= chase_speed;
    }
    if (control.down) {
        bat.acc.y += chase_speed;
    }
    if (!steering) {
        bat.vel = bat.vel * sim::ToSimScalar(0.8F);
    }
    if (bat.vel.x < sim::Scalar::zero()) {
        bat.facing = Side::Left;
    }
    if (bat.vel.x > sim::Scalar::zero()) {
        bat.facing = Side::Right;
    }
}

} // namespace

extern const EntSpec kBatSpec{
    .type_ = EntType::Bat,
    .size = EntSpecSize(8.0F, 8.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .impassable = false,
    .hurt_on_contact = true,
    .can_be_stunned = true,
    .draw_layer = DrawLayer::Middle,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::Vulnerable,
    .damage_anim = aframe_ids::BloodBall,
    .damage_sound = audio_asset_ids::BatSqueak,
    .collide_sound = audio_asset_ids::Thud,
    .control_logic = ControlEntAsBat,
    .step_logic = StepEntLogicAsBat,
    .step_physics = StepEntPhysicsAsBat,
    .ent_label_a = EntLabel::AttackThis,
    .alignment = Alignment::Enemy,
    .aframe_animator = AFrameAnimator::New(aframe_ids::HangingBat),
};

/** Bat goes up by default, and idles if it hits the ceiling.
 *  If the bat detects the player is beneath it,
 *  It checks if the player is within some dist below, some dist left or right.
 *      if yes, move towards the player right now.
 *  If no, give up and fly back to the ceiling.
 */
void StepEntLogicAsBat(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)graphics;
    (void)audio;
    (void)dt;
    Ent& bat = state.ents.ents[ent_idx];
    const bool controlled =
        state.controlled_ent_vid.has_value() && bat.vid == *state.controlled_ent_vid;
    if (controlled) {
        return;
    }

    const EntCondition bat_condition = bat.condition;

    if (bat_condition == EntCondition::Normal) {
        const std::optional<sim::FxVec2> target_position = FindBatTargetPosition(bat, state);

        //  State Machine
        Ent& mutable_bat = state.ents.ents[ent_idx];
        if (target_position.has_value()) {
            //  Chase The Player
            //  is this the begining of a pursuit?
            if (mutable_bat.ai_state == EntAiState::Idle) {
                //  squeak
                const AudioAssetId sound_effect =
                    state.drng.RandomIntInclusive(0, 1) == 0 ? audio_asset_ids::BatSqueak
                                                              : audio_asset_ids::BatFlap1;
                (void)PlayEntSoundEmitter(state, mutable_bat, sound_effect);
            }
            //  go to the target
            mutable_bat.ai_state = EntAiState::Pursuing;
            mutable_bat.acc += sim::NormalizeOrZero(*target_position - mutable_bat.pos) *
                               sim::ToSimScalar(kChaseSpeed);
            SetAnim(mutable_bat, aframe_ids::FlyingBat);
        } else {
            //  Go Back To Your Perch, (straight up from here lol)
            mutable_bat.ai_state = EntAiState::Returning;
            //  did you arrive at the perch
            const bool at_perch_or_roof = IsAtPerchOrRoof(mutable_bat, state);
            if (at_perch_or_roof) {
                mutable_bat.ai_state = EntAiState::Idle;
                mutable_bat.acc = sim::FxVec2::zero();
                mutable_bat.vel = sim::FxVec2::zero();
                SetAnim(mutable_bat, aframe_ids::HangingBat);
            } else {
                //  keep going up till you get there
                mutable_bat.acc += sim::FxVec2{
                    sim::Scalar::zero(),
                    sim::Scalar::from_int(-2),
                };
                mutable_bat.vel.x = sim::Scalar::zero();
                    SetAnim(mutable_bat, aframe_ids::FlyingBat);
            }
        }
        if (mutable_bat.vel.x < sim::Scalar::zero()) {
            mutable_bat.facing = Side::Left;
        }

        if (mutable_bat.vel.x > sim::Scalar::zero()) {
            mutable_bat.facing = Side::Right;
        }
    }
}

/** generalize this to all square or rectangular ents somehow */
void StepEntPhysicsAsBat(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)audio;
    Ent& bat = state.ents.ents[ent_idx];
    const bool controlled =
        state.controlled_ent_vid.has_value() && bat.vid == *state.controlled_ent_vid;
    const EntCondition bat_condition = bat.condition;
    const EntAiState bat_ai_state = bat.ai_state;
    const bool entered_idle_this_frame =
        bat_condition == EntCondition::Normal && bat_ai_state == EntAiState::Idle &&
        bat.last_ai_state != EntAiState::Idle;

    if (entered_idle_this_frame) {
        SnapBatToRoof(bat, state);
    }

    if (bat_condition != EntCondition::Normal) {
        common::ApplyGravity(ent_idx, state, dt);
    } else if (bat_ai_state == EntAiState::Idle) {
        bat.acc = sim::FxVec2::zero();
        bat.vel = sim::FxVec2::zero();
    } else if (!controlled) {
        common::ApplyGravity(ent_idx, state, dt);
    }

    common::PrePartialEulerStep(ent_idx, state, dt);
    if (bat_condition != EntCondition::Normal) {
        common::ApplySpecGroundFriction(ent_idx, state);
    } else if (controlled) {
        const sim::Scalar chase_max_speed = sim::ToSimScalar(kChaseMaxSpeed);
        bat.vel.x = gfxp::clamp(bat.vel.x, -chase_max_speed, chase_max_speed);
        bat.vel.y = gfxp::clamp(bat.vel.y, -chase_max_speed, chase_max_speed);
    } else if (bat.ai_state == EntAiState::Pursuing ||
        bat.ai_state == EntAiState::Returning) {
        const sim::Scalar chase_max_speed = sim::ToSimScalar(kChaseMaxSpeed);
        bat.vel.x = gfxp::clamp(bat.vel.x, -chase_max_speed, chase_max_speed);
        bat.vel.y = gfxp::clamp(bat.vel.y, -chase_max_speed, chase_max_speed);
    }
    common::DoTileAndEntCollisions(ent_idx, state, graphics, audio);
    if (bat_ai_state != EntAiState::Pursuing) {
    }
    common::PostPartialEulerStep(ent_idx, state, dt);
}

} // namespace splonks::ents::bat

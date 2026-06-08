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
#include <limits>
#include <vector>

namespace splonks::ents::bat {

namespace {

IAABB GetAreaAbove(const Ent& bat) {
    const auto [tl, br] = bat.GetBounds();
    return IAABB{
        .tl = IVec2::New(static_cast<int>(tl.x), static_cast<int>(tl.y) - 1),
        .br = IVec2::New(static_cast<int>(br.x), static_cast<int>(tl.y)),
    };
}

bool IsAtPerchOrRoof(const Ent& bat, const State& state) {
    const IAABB area_above = GetAreaAbove(bat);
    if (area_above.tl.y < 0) {
        return true;
    }
    for (const WorldTileQueryResult& tile_query : QueryTilesInWorldRect(state.stage, area_above.tl, area_above.br)) {
        if (tile_query.tile != nullptr && IsTileCollidable(*tile_query.tile)) {
            return true;
        }
    }
    return false;
}

std::optional<Vec2> FindBatTargetPosition(const Ent& bat, const State& state) {
    constexpr int kVerticalDetectDist = 8 * static_cast<int>(kTileSize);
    constexpr int kHorizontalChaseDist = 4 * static_cast<int>(kTileSize);

    std::optional<Vec2> best_target;
    float best_dist_sq = std::numeric_limits<float>::max();
    const Vec2 bat_pos = bat.pos;
    for (const PlayerSlot& slot : state.players.slots) {
        if (!slot.connected || !slot.ent_vid.has_value()) {
            continue;
        }

        const Ent* const player = state.ents.GetEnt(*slot.ent_vid);
        if (player == nullptr || !player->active || player->condition != EntCondition::Normal) {
            continue;
        }

        const Vec2 player_delta = GetNearestWorldDelta(state.stage, bat_pos, player->pos);
        if (player_delta.y <= 0.0F ||
            std::abs(player_delta.y) >= static_cast<float>(kVerticalDetectDist) ||
            std::abs(player_delta.x) >= static_cast<float>(kHorizontalChaseDist)) {
            continue;
        }

        const float dist_sq = player_delta.x * player_delta.x + player_delta.y * player_delta.y;
        if (dist_sq < best_dist_sq) {
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
        bat.pos.y -= 1.0F;
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
        bat.acc = Vec2::New(0.0F, 0.0F);
        bat.vel = Vec2::New(0.0F, 0.0F);
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
    bat.acc = Vec2::New(0.0F, 0.0F);
    if (control.left) {
        bat.acc.x -= kChaseSpeed;
    }
    if (control.right) {
        bat.acc.x += kChaseSpeed;
    }
    if (control.up) {
        bat.acc.y -= kChaseSpeed;
    }
    if (control.down) {
        bat.acc.y += kChaseSpeed;
    }
    if (!steering) {
        bat.vel = bat.vel * 0.8F;
    }
    if (bat.vel.x < 0.0F) {
        bat.facing = Side::Left;
    }
    if (bat.vel.x > 0.0F) {
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
        const std::optional<Vec2> target_position = FindBatTargetPosition(bat, state);

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
            mutable_bat.acc += NormalizeOrZeroDeterministic(*target_position - mutable_bat.pos) * kChaseSpeed;
            SetAnim(mutable_bat, aframe_ids::FlyingBat);
        } else {
            //  Go Back To Your Perch, (straight up from here lol)
            mutable_bat.ai_state = EntAiState::Returning;
            //  did you arrive at the perch
            const bool at_perch_or_roof = IsAtPerchOrRoof(mutable_bat, state);
            if (at_perch_or_roof) {
                mutable_bat.ai_state = EntAiState::Idle;
                mutable_bat.acc = Vec2::New(0.0F, 0.0F);
                mutable_bat.vel = Vec2::New(0.0F, 0.0F);
                SetAnim(mutable_bat, aframe_ids::HangingBat);
            } else {
                //  keep going up till you get there
                mutable_bat.acc += Vec2::New(0.0F, -2.0F);
                mutable_bat.vel.x = 0.0F;
                    SetAnim(mutable_bat, aframe_ids::FlyingBat);
            }
        }
        if (mutable_bat.vel.x < 0.0F) {
            mutable_bat.facing = Side::Left;
        }

        if (mutable_bat.vel.x > 0.0F) {
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
        bat.acc = Vec2::New(0.0F, 0.0F);
        bat.vel = Vec2::New(0.0F, 0.0F);
    } else if (!controlled) {
        common::ApplyGravity(ent_idx, state, dt);
    }

    common::PrePartialEulerStep(ent_idx, state, dt);
    if (bat_condition != EntCondition::Normal) {
        common::ApplySpecGroundFriction(ent_idx, state);
    } else if (controlled) {
        bat.vel.x = std::clamp(bat.vel.x, -kChaseMaxSpeed, kChaseMaxSpeed);
        bat.vel.y = std::clamp(bat.vel.y, -kChaseMaxSpeed, kChaseMaxSpeed);
    } else if (bat.ai_state == EntAiState::Pursuing ||
        bat.ai_state == EntAiState::Returning) {
        bat.vel.x = std::clamp(bat.vel.x, -kChaseMaxSpeed, kChaseMaxSpeed);
        bat.vel.y = std::clamp(bat.vel.y, -kChaseMaxSpeed, kChaseMaxSpeed);
    }
    common::DoTileAndEntCollisions(ent_idx, state, graphics, audio);
    if (bat_ai_state != EntAiState::Pursuing) {
    }
    common::PostPartialEulerStep(ent_idx, state, dt);
}

} // namespace splonks::ents::bat

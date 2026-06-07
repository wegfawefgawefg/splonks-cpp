#include "ents/monkey.hpp"

#include "audio.hpp"
#include "ent/spec.hpp"
#include "ents/common/common.hpp"
#include "ents/common/ground_walker.hpp"
#include "aframe_id.hpp"
#include "player_queries.hpp"
#include "state.hpp"
#include "tools/tool_spec.hpp"
#include "world_ops.hpp"
#include "world_query.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>

namespace splonks::ents::monkey {

namespace {

enum class MonkeyState {
    Idle = 0,
    Bounce = 1,
    Recover = 2,
    Charge = 3,
    Hang = 5,
    Climb = 6,
    Grab = 7,
};

constexpr float kMonkeyGroundLeapSpeed = 2.0F;
constexpr float kMonkeyLeapSpeed = 3.0F;
constexpr float kMonkeyClimbSpeed = 1.0F;
constexpr float kMonkeySightDistance = 64.0F;
constexpr float kMonkeySightDistanceSq = kMonkeySightDistance * kMonkeySightDistance;
constexpr float kMonkeyItemThrowSpeedX = 5.0F;
constexpr float kMonkeyItemThrowSpeedY = -4.0F;
constexpr int kMonkeyIdleMinFrames = 18;
constexpr int kMonkeyIdleMaxFrames = 56;
constexpr int kMonkeyChargeMinFrames = 10;
constexpr int kMonkeyChargeMaxFrames = 24;
constexpr int kMonkeyClimbChanceFromHangPercent = 65;
constexpr int kMonkeyGroundClimbChancePercent = 100;
constexpr int kMonkeyClimbUpBiasPercent = 70;
constexpr int kMonkeyClimbSearchRadiusXTiles = 16;
constexpr int kMonkeyClimbSearchUpTiles = 16;
constexpr int kMonkeyClimbSearchDownTiles = 2;
constexpr int kMonkeyClimbMinFrames = 60;
constexpr int kMonkeyClimbMaxFrames = 160;
constexpr int kMonkeyClimbSideDismountPercent = 35;
constexpr std::uint32_t kMonkeyGrabCooldownFrames = 60;
constexpr std::uint32_t kMonkeyThrowCooldownFrames = 60;
constexpr std::uint32_t kMonkeyVineCooldownFrames = 30;
constexpr std::uint32_t kMonkeyClimbDismountCooldownFrames = 8;
constexpr std::uint32_t kMonkeyTripStunFrames = 40;
constexpr std::uint32_t kMonkeyRobMoneyAmount = 500;
constexpr int kMonkeyPlayerAttachSwapFrames = 8;
constexpr float kMonkeyPlayerAttachOffsetX = 5.0F;
constexpr float kMonkeyPlayerAttachOffsetY = -2.0F;

MonkeyState GetMonkeyState(const Ent& monkey) {
    return static_cast<MonkeyState>(static_cast<int>(monkey.counter_d));
}

void SetMonkeyState(Ent& monkey, MonkeyState state) {
    monkey.counter_d = static_cast<float>(static_cast<int>(state));
}

bool IsClimbableAt(const State& state, const Vec2& world_pos) {
    const std::optional<WorldTileQueryResult> tile_query =
        QueryTileAtWorldPos(state.stage, ToIVec2(world_pos));
    return tile_query.has_value() && IsTileQueryClimbable(state.stage, *tile_query);
}

std::optional<Vec2> FindNearbyClimbableCenter(const State& state, const Ent& monkey) {
    const Vec2 center = monkey.GetCenter();
    constexpr float kProbeXs[] = {0.0F, -6.0F, 6.0F, -10.0F, 10.0F};
    for (const float probe_x : kProbeXs) {
        const std::optional<WorldTileQueryResult> tile_query =
            QueryTileAtWorldPos(state.stage, ToIVec2(center + Vec2::New(probe_x, 0.0F)));
        if (!tile_query.has_value() || !IsTileQueryClimbable(state.stage, *tile_query)) {
            continue;
        }

        return Vec2::New(
            (static_cast<float>(tile_query->tile_pos.x) + 0.5F) * static_cast<float>(kTileSize),
            center.y
        );
    }
    return std::nullopt;
}

std::optional<Vec2> FindClimbableTargetCenter(const State& state, const Ent& monkey) {
    const Vec2 center = monkey.GetCenter();
    const std::optional<WorldTileQueryResult> origin_query =
        QueryTileAtWorldPos(state.stage, ToIVec2(center));
    if (!origin_query.has_value()) {
        return std::nullopt;
    }

    float best_score = 0.0F;
    std::optional<Vec2> best_center;
    for (int dy = -kMonkeyClimbSearchUpTiles; dy <= kMonkeyClimbSearchDownTiles; ++dy) {
        for (int dx = -kMonkeyClimbSearchRadiusXTiles; dx <= kMonkeyClimbSearchRadiusXTiles; ++dx) {
            if (dx == 0 && dy == 0) {
                continue;
            }

            const std::optional<WorldTileQueryResult> tile_query = QueryTileAtTilePos(
                state.stage,
                IVec2::New(origin_query->tile_pos.x + dx, origin_query->tile_pos.y + dy)
            );
            if (!tile_query.has_value() || !IsTileQueryClimbable(state.stage, *tile_query)) {
                continue;
            }

            const float downward_penalty = dy > 0 ? 64.0F : 0.0F;
            const float score =
                std::abs(static_cast<float>(dx)) +
                std::abs(static_cast<float>(dy)) * 1.5F +
                downward_penalty;
            if (!best_center.has_value() || score < best_score) {
                best_score = score;
                best_center = Vec2::New(
                    (static_cast<float>(tile_query->tile_pos.x) + 0.5F) *
                        static_cast<float>(kTileSize),
                    (static_cast<float>(tile_query->tile_pos.y) + 0.5F) *
                        static_cast<float>(kTileSize)
                );
            }
        }
    }

    return best_center;
}

bool ShouldClimbUp(Ent& monkey, State& state, const std::optional<Vec2>& player_delta) {
    (void)monkey;
    if (player_delta.has_value() && LengthSquared(*player_delta) < kMonkeySightDistanceSq &&
        std::abs(player_delta->y) > 16.0F) {
        return player_delta->y < 0.0F;
    }
    return state.drng.RandomIntInclusive(1, 100) <= kMonkeyClimbUpBiasPercent;
}

std::optional<Vec2> GetNearestPlayerDelta(const Ent& monkey, const State& state) {
    const Ent* const player = FindNearestPlayer(state, monkey.GetCenter(), false);
    if (player == nullptr || player->condition == EntCondition::Dead) {
        return std::nullopt;
    }
    return GetNearestWorldDelta(state.stage, monkey.GetCenter(), player->GetCenter());
}

AudioAssetId RandomMonkeyNoise(State& state, int min_index, int max_index) {
    switch (state.drng.RandomIntInclusive(min_index, max_index)) {
    case 1:
        return audio_asset_ids::MonkeyNoise1;
    case 2:
        return audio_asset_ids::MonkeyNoise2;
    case 3:
        return audio_asset_ids::MonkeyNoise3;
    case 4:
        return audio_asset_ids::MonkeyNoise4;
    default:
        return audio_asset_ids::MonkeyNoise5;
    }
}

void ThrowSpawnedEnt(Ent& ent, const Ent& monkey) {
    const float throw_x = monkey.facing == Side::Right
                              ? kMonkeyItemThrowSpeedX
                              : -kMonkeyItemThrowSpeedX;
    ent.vel = Vec2::New(throw_x, kMonkeyItemThrowSpeedY);
    ent.acc = Vec2::New(0.0F, 0.0F);
    ent.thrown_by = monkey.vid;
    ent.thrown_immunity_timer = common::kThrownByImmunityDuration;
    ent.proj_contact_timer = common::kProjContactDuration;
}

Vec2 BuildMonkeyThrowLeft(const controls::ControlIntent&) {
    return Vec2::New(-kMonkeyItemThrowSpeedX, kMonkeyItemThrowSpeedY);
}

Vec2 BuildMonkeyThrowRight(const controls::ControlIntent&) {
    return Vec2::New(kMonkeyItemThrowSpeedX, kMonkeyItemThrowSpeedY);
}

bool CanMonkeyStealToolSlot(const ToolSlot& slot) {
    if (!slot.active || slot.count == 0) {
        return false;
    }
    return GetToolSpec(slot.kind).use_fn != nullptr;
}

bool TryStealRandomToolAndCast(
    std::size_t monkey_idx,
    const Ent& monkey,
    const Ent& player,
    State& state,
    Graphics& graphics,
    Audio& audio
) {
    const EntToolState* const tool_state = state.ent_tools.FindEntToolState(player.vid);
    if (tool_state == nullptr) {
        return false;
    }

    std::array<std::uint32_t, kToolSlotCount> stealable_slot_indices{};
    std::uint32_t stealable_slot_count = 0;
    for (std::size_t slot_index = 0; slot_index < tool_state->slots.size(); ++slot_index) {
        if (!CanMonkeyStealToolSlot(tool_state->slots[slot_index])) {
            continue;
        }
        stealable_slot_indices[static_cast<std::size_t>(stealable_slot_count)] =
            static_cast<std::uint32_t>(slot_index);
        stealable_slot_count += 1;
    }
    if (stealable_slot_count == 0) {
        return false;
    }

    const std::uint32_t selected_slot_index = stealable_slot_indices[static_cast<std::size_t>(
        state.drng.RandomIntInclusive(0, static_cast<int>(stealable_slot_count - 1))
    )];
    const std::size_t selected_slot_array_index = static_cast<std::size_t>(selected_slot_index);
    const ToolKind stolen_kind = tool_state->slots[selected_slot_array_index].kind;

    ToolSlot& monkey_slot = state.ent_tools.EnsureToolSlot(monkey.vid, 0);
    const ToolSlot previous_monkey_slot = monkey_slot;
    FillToolSlot(monkey_slot, stolen_kind, 1, true);
    monkey_slot.cooldown = 0;

    const ToolThrowVelocityBuilder throw_builder =
        monkey.facing == Side::Right ? BuildMonkeyThrowRight : BuildMonkeyThrowLeft;
    const bool cast = common::TryUseToolSlot(monkey_idx, state, graphics, audio, 0, true, throw_builder);
    monkey_slot = previous_monkey_slot;
    if (!cast) {
        return false;
    }

    EntToolState* const mutable_player_tool_state = state.ent_tools.FindEntToolStateMut(player.vid);
    if (mutable_player_tool_state == nullptr ||
        selected_slot_array_index >= mutable_player_tool_state->slots.size()) {
        return true;
    }

    ToolSlot& player_slot = mutable_player_tool_state->slots[selected_slot_array_index];
    if (player_slot.active && player_slot.kind == stolen_kind && player_slot.count > 0) {
        player_slot.count -= 1;
    }
    return true;
}

void BounceAwayFromPlayer(Ent& monkey, const Ent& player, State& state) {
    const Vec2 delta = GetNearestWorldDelta(state.stage, monkey.GetCenter(), player.GetCenter());
    monkey.draw_layer = DrawLayer::Foreground;
    monkey.vel.y = -static_cast<float>(state.drng.RandomIntInclusive(2, 4));
    if (delta.x > 0.0F) {
        monkey.facing = Side::Left;
        monkey.vel.x = -kMonkeyLeapSpeed;
    } else {
        monkey.facing = Side::Right;
        monkey.vel.x = kMonkeyLeapSpeed;
    }
    SetMonkeyState(monkey, MonkeyState::Bounce);
    monkey.counter_c = static_cast<float>(kMonkeyVineCooldownFrames);
    monkey.counter_b = static_cast<float>(kMonkeyGrabCooldownFrames);
    TrySetAnim(monkey, EntDisplayState::Falling);
}

void EnterIdle(Ent& monkey, State& state) {
    SetMonkeyState(monkey, MonkeyState::Idle);
    monkey.draw_layer = DrawLayer::Foreground;
    monkey.counter_a = static_cast<float>(
        state.drng.RandomIntInclusive(kMonkeyIdleMinFrames, kMonkeyIdleMaxFrames));
    monkey.point_a.x = 0;
    monkey.vel.x = 0.0F;
    TrySetAnim(monkey, EntDisplayState::Neutral);
}

void EnterCharge(Ent& monkey, State& state) {
    SetMonkeyState(monkey, MonkeyState::Charge);
    monkey.counter_a = static_cast<float>(
        state.drng.RandomIntInclusive(kMonkeyChargeMinFrames, kMonkeyChargeMaxFrames));
    monkey.vel.x = 0.0F;
    TrySetAnim(monkey, EntDisplayState::Walk);
}

void EnterHang(Ent& monkey, State& state, int forced_launch_direction = 0) {
    SetMonkeyState(monkey, MonkeyState::Hang);
    monkey.vel = Vec2::New(0.0F, 0.0F);
    monkey.acc = Vec2::New(0.0F, 0.0F);
    monkey.point_a.x = std::clamp(forced_launch_direction, -1, 1);
    monkey.counter_a = static_cast<float>(state.drng.RandomIntInclusive(10, 40));
    TrySetAnim(monkey, EntDisplayState::Hanging);
}

void EnterClimb(Ent& monkey, State& state, bool moving_up) {
    SetMonkeyState(monkey, MonkeyState::Climb);
    monkey.vel = Vec2::New(0.0F, 0.0F);
    monkey.acc = Vec2::New(0.0F, 0.0F);
    monkey.point_a.x = moving_up ? 0 : 1;
    monkey.counter_a = static_cast<float>(
        state.drng.RandomIntInclusive(kMonkeyClimbMinFrames, kMonkeyClimbMaxFrames));
    TrySetAnim(monkey, EntDisplayState::Climbing);
}

void SnapToClimbableAndEnterClimb(Ent& monkey, State& state, const Vec2& climb_center, bool moving_up) {
    monkey.SetCenter(Vec2::New(climb_center.x, monkey.GetCenter().y));
    EnterClimb(monkey, state, moving_up);
}

bool TryGroundClimbOrApproach(Ent& monkey, State& state, const std::optional<Vec2>& player_delta) {
    const std::optional<Vec2> climb_center = FindNearbyClimbableCenter(state, monkey);
    if (climb_center.has_value()) {
        SnapToClimbableAndEnterClimb(monkey, state, *climb_center, ShouldClimbUp(monkey, state, player_delta));
        return true;
    }

    const std::optional<Vec2> climb_target = FindClimbableTargetCenter(state, monkey);
    if (!climb_target.has_value()) {
        return false;
    }

    const Vec2 delta = GetNearestWorldDelta(state.stage, monkey.GetCenter(), *climb_target);
    if (std::abs(delta.x) < 3.0F) {
        SnapToClimbableAndEnterClimb(monkey, state, *climb_target, ShouldClimbUp(monkey, state, player_delta));
        return true;
    }

    const float direction = delta.x < 0.0F ? -1.0F : 1.0F;
    monkey.facing = direction < 0.0F ? Side::Left : Side::Right;
    monkey.draw_layer = DrawLayer::Foreground;
    monkey.vel.x = direction * kMonkeyGroundLeapSpeed;
    monkey.vel.y =
        -static_cast<float>(state.drng.RandomIntInclusive(delta.y < -8.0F ? 6 : 5, 7));
    monkey.point_a.x = 0;
    SetMonkeyState(monkey, MonkeyState::Recover);
    TrySetAnim(monkey, EntDisplayState::Falling);
    return true;
}

void LaunchAtPlayerOrForward(Ent& monkey, const std::optional<Vec2>& player_delta, State& state) {
    monkey.draw_layer = DrawLayer::Foreground;
    const int forced_launch_direction = std::clamp(monkey.point_a.x, -1, 1);
    if (forced_launch_direction != 0) {
        monkey.facing = forced_launch_direction < 0 ? Side::Left : Side::Right;
        monkey.vel.x = static_cast<float>(forced_launch_direction) * kMonkeyGroundLeapSpeed;
    } else if (player_delta.has_value() && player_delta->x < 0.0F) {
        monkey.facing = Side::Left;
        monkey.vel.x = -kMonkeyGroundLeapSpeed;
    } else if (player_delta.has_value()) {
        monkey.facing = Side::Right;
        monkey.vel.x = kMonkeyGroundLeapSpeed;
    } else {
        monkey.vel.x = monkey.facing == Side::Left ? -kMonkeyGroundLeapSpeed : kMonkeyGroundLeapSpeed;
    }
    monkey.point_a.x = 0;
    const bool needs_height = monkey.grounded || forced_launch_direction != 0;
    monkey.vel.y = -static_cast<float>(
        state.drng.RandomIntInclusive(needs_height ? 5 : 4, needs_height ? 7 : 5));
    SetMonkeyState(monkey, MonkeyState::Recover);
    TrySetAnim(monkey, EntDisplayState::Falling);
    (void)PlayEntCenterSoundEmitter(state, monkey, RandomMonkeyNoise(state, 3, 5));
}

void LaunchOffClimbable(Ent& monkey, const std::optional<Vec2>& player_delta, State& state) {
    if (player_delta.has_value() && LengthSquared(*player_delta) < kMonkeySightDistanceSq) {
        LaunchAtPlayerOrForward(monkey, player_delta, state);
    } else {
        monkey.facing =
            state.drng.RandomIntInclusive(0, 1) == 0 ? Side::Left : Side::Right;
        LaunchAtPlayerOrForward(monkey, std::nullopt, state);
    }
    monkey.counter_c = static_cast<float>(kMonkeyClimbDismountCooldownFrames);
}

void HopAlongClimbable(Ent& monkey, const std::optional<Vec2>& player_delta, State& state) {
    const bool moving_up = ShouldClimbUp(monkey, state, player_delta);
    monkey.point_a.x = moving_up ? 0 : 1;
    monkey.vel = Vec2::New(0.0F, moving_up ? -kMonkeyClimbSpeed : kMonkeyClimbSpeed);
    monkey.acc = Vec2::New(0.0F, 0.0F);
    monkey.counter_a = static_cast<float>(
        state.drng.RandomIntInclusive(kMonkeyClimbMinFrames, kMonkeyClimbMaxFrames));
}

void TripPlayer(Ent& player, State& state) {
    const float trip_x = player.facing == Side::Left ? -3.0F : 3.0F;
    player.vel = Vec2::New(trip_x, -3.0F);
    player.acc = Vec2::New(0.0F, 0.0F);
    player.condition = EntCondition::Stunned;
    player.stun_timer = kMonkeyTripStunFrames;
    common::DropHeldItemFromEnt(player, state);
    TrySetAnim(player, EntDisplayState::Stunned);
}

void RobPlayer(std::size_t monkey_idx, Ent& monkey, Ent& player, State& state, Graphics& graphics, Audio& audio) {
    if (state.drng.RandomIntInclusive(1, 4) == 1) {
        TripPlayer(player, state);
        (void)PlayEntCenterSoundEmitter(state, player, audio_asset_ids::Thud);
    } else if (player.money >= kMonkeyRobMoneyAmount &&
               state.drng.RandomIntInclusive(1, 10) <= 8) {
        player.money -= kMonkeyRobMoneyAmount;
        if (world_ops::SpawnEnt(state, EntType::GoldNugget, [&](Ent& gold) {
                gold.SetCenter(monkey.GetCenter());
                state.UpdateSidForEnt(gold.vid.id, graphics);
                gold.vel = Vec2::New(
                    static_cast<float>(state.drng.RandomIntInclusive(-2, 2)),
                    -static_cast<float>(state.drng.RandomIntInclusive(3, 4))
                );
                gold.acc = Vec2::New(0.0F, 0.0F);
            }) != nullptr) {
        }
        (void)PlayEntCenterSoundEmitter(state, monkey, audio_asset_ids::Throw);
    } else {
        (void)TryStealRandomToolAndCast(monkey_idx, monkey, player, state, graphics, audio);
    }

    BounceAwayFromPlayer(monkey, player, state);
    (void)PlayEntCenterSoundEmitter(state, monkey, RandomMonkeyNoise(state, 1, 4));
}

void AttachToPlayer(Ent& monkey, Ent& player, State& state) {
    monkey.ent_a = player.vid;
    monkey.point_a = IVec2::New(1, 0);
    monkey.vel = Vec2::New(0.0F, 0.0F);
    monkey.acc = Vec2::New(0.0F, 0.0F);
    monkey.counter_a = static_cast<float>(state.drng.RandomIntInclusive(40, 80));
    monkey.counter_b = static_cast<float>(kMonkeyGrabCooldownFrames);
    SetMonkeyState(monkey, MonkeyState::Grab);
    monkey.draw_layer = DrawLayer::Background;
    TrySetAnim(monkey, EntDisplayState::Hanging);
    (void)PlayEntCenterSoundEmitter(state, monkey, audio_asset_ids::MonkeyNoise2);
}

void UpdateAnim(Ent& monkey) {
    switch (GetMonkeyState(monkey)) {
    case MonkeyState::Hang:
        TrySetAnim(monkey, EntDisplayState::Hanging);
        return;
    case MonkeyState::Climb:
    case MonkeyState::Grab:
        TrySetAnim(monkey, EntDisplayState::Climbing);
        return;
    case MonkeyState::Charge:
        TrySetAnim(monkey, EntDisplayState::Walk);
        return;
    case MonkeyState::Bounce:
    case MonkeyState::Recover:
        TrySetAnim(monkey, EntDisplayState::Falling);
        return;
    case MonkeyState::Idle:
        TrySetAnim(monkey, EntDisplayState::Neutral);
        return;
    }
}

} // namespace

void StepEntLogicAsMonkey(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)dt;
    Ent& monkey = state.ents.ents[ent_idx];
    if (monkey.last_condition == EntCondition::Stunned &&
        monkey.condition == EntCondition::Normal) {
        EnterIdle(monkey, state);
    }
    if (monkey.condition != EntCondition::Normal) {
        monkey.draw_layer = DrawLayer::Foreground;
        return;
    }

    if (monkey.counter_b > 0.0F) {
        monkey.counter_b -= 1.0F;
    }
    if (monkey.counter_c > 0.0F) {
        monkey.counter_c -= 1.0F;
    }
    if (monkey.threshold_a > sim::Scalar::zero()) {
        monkey.threshold_a -= sim::Scalar::from_int(1);
    }

    const std::optional<Vec2> player_delta = GetNearestPlayerDelta(monkey, state);
    MonkeyState monkey_state = GetMonkeyState(monkey);

    switch (monkey_state) {
    case MonkeyState::Idle:
        common::DecelerateHorizontallyToStop(monkey, 0.5F);
        if (monkey.counter_a > 0.0F) {
            monkey.counter_a -= 1.0F;
        } else if (monkey.grounded &&
                   state.drng.RandomIntInclusive(1, 100) <= kMonkeyGroundClimbChancePercent &&
                   TryGroundClimbOrApproach(monkey, state, player_delta)) {
        } else {
            EnterCharge(monkey, state);
        }
        if (GetMonkeyState(monkey) == MonkeyState::Idle &&
            player_delta.has_value() && LengthSquared(*player_delta) < kMonkeySightDistanceSq) {
            EnterCharge(monkey, state);
        }
        break;
    case MonkeyState::Charge:
        common::DecelerateHorizontallyToStop(monkey, 0.5F);
        if (monkey.counter_a > 0.0F) {
            monkey.counter_a -= 1.0F;
        } else if (monkey.grounded &&
                   state.drng.RandomIntInclusive(1, 100) <= kMonkeyGroundClimbChancePercent &&
                   TryGroundClimbOrApproach(monkey, state, player_delta)) {
        } else {
            LaunchAtPlayerOrForward(monkey, player_delta, state);
        }
        break;
    case MonkeyState::Recover:
        if (monkey.grounded) {
            EnterIdle(monkey, state);
        } else if (monkey.counter_c <= 0.0F && IsClimbableAt(state, monkey.GetCenter())) {
            EnterHang(monkey, state);
        } else if (monkey.collided_last_frame) {
            const bool wall_left = common::HasWallAheadForGroundWalker(monkey, state, graphics, -1);
            const bool wall_right = common::HasWallAheadForGroundWalker(monkey, state, graphics, 1);
            if (wall_left || wall_right) {
                monkey.facing = wall_right ? Side::Left : Side::Right;
                const int launch_direction = wall_right ? -1 : 1;
                EnterHang(monkey, state, launch_direction);
            }
        }
        break;
    case MonkeyState::Bounce:
        if (monkey.grounded) {
            EnterCharge(monkey, state);
        } else {
            SetMonkeyState(monkey, MonkeyState::Recover);
        }
        break;
    case MonkeyState::Hang:
        monkey.vel = Vec2::New(0.0F, 0.0F);
        monkey.acc = Vec2::New(0.0F, 0.0F);
        if (monkey.counter_a > 0.0F) {
            monkey.counter_a -= 1.0F;
        } else if (monkey.point_a.x == 0 &&
                   state.drng.RandomIntInclusive(1, 100) <= kMonkeyClimbChanceFromHangPercent) {
            const std::optional<Vec2> climb_center = FindNearbyClimbableCenter(state, monkey);
            if (climb_center.has_value()) {
                SnapToClimbableAndEnterClimb(monkey, state, *climb_center, ShouldClimbUp(monkey, state, player_delta));
            } else {
                EnterCharge(monkey, state);
            }
        } else {
            EnterCharge(monkey, state);
        }
        break;
    case MonkeyState::Climb: {
        monkey.vel.x = 0.0F;
        const bool moving_up = monkey.point_a.x == 0;
        monkey.vel.y = moving_up ? -kMonkeyClimbSpeed : kMonkeyClimbSpeed;
        const Vec2 probe = monkey.GetCenter() + Vec2::New(0.0F, moving_up ? -8.0F : 14.0F);
        if (monkey.counter_a > 0.0F) {
            monkey.counter_a -= 1.0F;
        } else if (state.drng.RandomIntInclusive(1, 100) <= kMonkeyClimbSideDismountPercent) {
            LaunchOffClimbable(monkey, player_delta, state);
            break;
        } else {
            HopAlongClimbable(monkey, player_delta, state);
        }

        if (!IsClimbableAt(state, probe)) {
            monkey.point_a.x = moving_up ? 1 : 0;
            EnterHang(monkey, state);
        }

        if (player_delta.has_value() && LengthSquared(*player_delta) < kMonkeySightDistanceSq &&
            player_delta->y > 0.0F) {
            SetMonkeyState(monkey, MonkeyState::Bounce);
            monkey.counter_c = static_cast<float>(kMonkeyVineCooldownFrames);
            monkey.vel.y = -static_cast<float>(state.drng.RandomIntInclusive(2, 4));
            if (player_delta->x < 0.0F) {
                monkey.facing = Side::Left;
                monkey.vel.x = -kMonkeyLeapSpeed;
            } else {
                monkey.facing = Side::Right;
                monkey.vel.x = kMonkeyLeapSpeed;
            }
        }
        break;
    }
    case MonkeyState::Grab: {
        if (!monkey.ent_a.has_value()) {
            SetMonkeyState(monkey, MonkeyState::Bounce);
            break;
        }
        Ent* const player = state.ents.GetEntMut(*monkey.ent_a);
        if (player == nullptr || !player->active || player->condition == EntCondition::Dead) {
            monkey.ent_a.reset();
            SetMonkeyState(monkey, MonkeyState::Bounce);
            break;
        }

        monkey.vel = Vec2::New(0.0F, 0.0F);
        monkey.acc = Vec2::New(0.0F, 0.0F);
        const int side = ((state.stage_frame / kMonkeyPlayerAttachSwapFrames) % 2U) == 0U ? -1 : 1;
        monkey.point_a.x = side;
        monkey.facing = side < 0 ? Side::Right : Side::Left;
        monkey.SetCenter(
            player->GetCenter() +
            Vec2::New(static_cast<float>(side) * kMonkeyPlayerAttachOffsetX, kMonkeyPlayerAttachOffsetY)
        );
        if (monkey.counter_a > 0.0F) {
            monkey.counter_a -= 1.0F;
        } else {
            RobPlayer(ent_idx, monkey, *player, state, graphics, audio);
            monkey.ent_a.reset();
        }
        break;
    }
    }

    UpdateAnim(monkey);
}

void StepEntPhysicsAsMonkey(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    Ent& monkey = state.ents.ents[ent_idx];
    if (monkey.condition != EntCondition::Normal) {
        common::StepStandardPhysics(ent_idx, state, graphics, audio, dt);
        return;
    }

    switch (GetMonkeyState(monkey)) {
    case MonkeyState::Grab:
    case MonkeyState::Hang:
        monkey.vel = Vec2::New(0.0F, 0.0F);
        monkey.acc = Vec2::New(0.0F, 0.0F);
        return;
    case MonkeyState::Climb:
        common::PrePartialEulerStep(ent_idx, state, dt);
        common::DoTileAndEntCollisions(ent_idx, state, graphics, audio);
        common::PostPartialEulerStep(ent_idx, state, dt);
        return;
    default:
        common::StepStandardPhysics(ent_idx, state, graphics, audio, dt);
        return;
    }
}

common::ContactResult OnEntContactAsMonkey(
    std::size_t ent_idx,
    std::size_t other_ent_idx,
    const common::ContactContext&,
    State& state,
    const Graphics* graphics,
    Audio* audio
) {
    if (graphics == nullptr || audio == nullptr) {
        return {};
    }

    Ent& monkey = state.ents.ents[ent_idx];
    Ent& other = state.ents.ents[other_ent_idx];
    if (monkey.condition != EntCondition::Normal || !other.active) {
        return {};
    }
    if (GetMonkeyState(monkey) == MonkeyState::Grab) {
        return {};
    }

    if (state.players.FindByEntVid(other.vid) != nullptr &&
        monkey.counter_b <= 0.0F && other.condition == EntCondition::Normal) {
        AttachToPlayer(monkey, other, state);
        return {};
    }

    if (monkey.threshold_a <= sim::Scalar::zero() && other.can_be_picked_up && !other.held_by_vid.has_value() &&
        other.attach_mode == AttachMode::None && other.type_ != EntType::Monkey &&
        other.type_ != EntType::Player && !other.impassable) {
        ThrowSpawnedEnt(other, monkey);
        monkey.threshold_a = sim::Scalar::from_int(kMonkeyThrowCooldownFrames);
        SetMonkeyState(monkey, MonkeyState::Idle);
        monkey.counter_a = static_cast<float>(state.drng.RandomIntInclusive(20, 60));
    }

    return {};
}

extern const EntSpec kMonkeySpec{
    .type_ = EntType::Monkey,
    .size = Vec2::New(8.0F, 10.0F),
    .health = 1,
    .has_physics = true,
    .can_collide = true,
    .can_be_picked_up = true,
    .can_only_be_picked_up_if_dead_or_stunned = true,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stunned = true,
    .draw_layer = DrawLayer::Foreground,
    .facing = Side::Left,
    .condition = EntCondition::Normal,
    .ai_state = EntAiState::Idle,
    .display_state = EntDisplayState::Hanging,
    .counter_a = 20.0F,
    .counter_b = static_cast<float>(kMonkeyGrabCooldownFrames),
    .counter_d = static_cast<float>(static_cast<int>(MonkeyState::Hang)),
    .damage_vuln = DamageVuln::Vulnerable,
    .damage_anim = aframe_ids::BloodBall,
    .damage_sound = audio_asset_ids::MonkeyNoise1,
    .collide_sound = audio_asset_ids::Thud,
    .death_sound = audio_asset_ids::MonkeyNoise2,
    .step_logic = StepEntLogicAsMonkey,
    .step_physics = StepEntPhysicsAsMonkey,
    .on_ent_contact = OnEntContactAsMonkey,
    .alignment = Alignment::Enemy,
    .aframe_animator = AFrameAnimator::New(aframe_ids::MonkeyHang),
};

} // namespace splonks::ents::monkey

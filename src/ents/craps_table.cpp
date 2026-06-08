#include "ents/craps_table.hpp"

#include "audio_emitters.hpp"
#include "buying.hpp"
#include "ents/common/common.hpp"
#include "ent.hpp"
#include "ent/spec.hpp"
#include "aframe_animator.hpp"
#include "aframe_id.hpp"
#include "player_queries.hpp"
#include "sim/fxp.hpp"
#include "state.hpp"
#include "utils.hpp"
#include "world_ops.hpp"
#include "world_query.hpp"

#include <algorithm>
#include <cmath>

namespace splonks::ents::craps_table {

namespace {

constexpr std::uint32_t kCrapsBetAmount = 1000;
constexpr int kDiceRollState = 1;
constexpr float kDiceSettleSpeed = 0.2F;
constexpr float kResultPromptFrames = 120.0F;
constexpr float kPrizeAlphaLocked = 0.55F;

enum class TableState {
    Idle = 0,
    Rolling = 1,
    Result = 2,
};

TableState GetTableState(const Ent& table) {
    return static_cast<TableState>(table.counter_a.trunc_int());
}

sim::Vec2 PrizeLaunchVelocity() {
    return sim::Vec2{
        sim::Scalar::zero(),
        sim::ToSimScalar(-2.25F),
    };
}

sim::Vec2 DiceLaunchVelocity(State& state) {
    return sim::Vec2{
        sim::Scalar::from_int(state.drng.RandomIntInclusive(-2, 2)),
        sim::Scalar::from_int(-5),
    };
}

void SetTableState(Ent& table, TableState state) {
    table.counter_a = sim::Scalar::from_int(static_cast<int>(state));
}

Ent* GetLinkedEnt(State& state, std::optional<VID> vid) {
    if (!vid.has_value()) {
        return nullptr;
    }
    return state.ents.GetEntMut(*vid);
}

bool IsShopDisturbed(const Ent& table, const State& state) {
    const Ent* const shop = table.ent_a.has_value()
        ? state.ents.GetEnt(*table.ent_a)
        : nullptr;
    return shop != nullptr && shop->active && shop->ai_state == EntAiState::Disturbed;
}

int RollDicePairTotal(State& state) {
    return state.drng.RandomIntInclusive(1, 6) +
           state.drng.RandomIntInclusive(1, 6);
}

void LockPrize(Ent& prize) {
    ClearEntBuyableState(prize);
    prize.has_physics = false;
    prize.can_collide = false;
    prize.can_be_picked_up = false;
    prize.can_be_hit = false;
    prize.alpha = sim::ToSimScalar(kPrizeAlphaLocked);
}

void UnlockPrize(Ent& prize) {
    const EntSpec& spec = GetEntSpec(prize.type_);
    prize.has_physics = spec.has_physics;
    prize.can_collide = spec.can_collide;
    prize.can_be_picked_up = spec.can_be_picked_up;
    prize.can_be_hit = spec.can_be_hit;
    prize.alpha = spec.alpha;
    prize.vel = PrizeLaunchVelocity();
    prize.acc = sim::Vec2::zero();
}

Ent* SpawnWonPrize(Ent& table, const Ent& display_prize, State& state) {
    Ent* const won_prize = world_ops::SpawnEnt(
        state,
        display_prize.type_,
        [&](Ent& ent) {
            ClearEntBuyableState(ent);
            ent.SetSimCenter(table.GetSimCenter() + sim::Vec2::from_pixels(0, -18));
            ent.vel = PrizeLaunchVelocity();
            ent.acc = sim::Vec2::zero();
            ent.grounded = false;
        }
    );
    return won_prize;
}

void PrepareCrapsDice(Ent& dice) {
    dice.can_be_picked_up = false;
    dice.can_apply_proj_contact = false;
    dice.proj_contact_timer = 0;
    dice.thrown_by.reset();
    dice.held_by_vid.reset();
    dice.attach_mode = AttachMode::None;
}

void LaunchDice(Ent& table, Ent& dice, State& state) {
    PrepareCrapsDice(dice);
    dice.has_physics = true;
    dice.can_collide = true;
    dice.grounded = false;
    dice.counter_a = sim::Scalar::from_int(RollDicePairTotal(state));
    dice.counter_b = sim::Scalar::from_int(kDiceRollState);
    dice.rotation = sim::ToSimScalar(static_cast<float>(state.drng.RandomIntInclusive(0, 359)));

    dice.SetSimCenter(table.GetSimCenter() + sim::Vec2::from_pixels(0, -10));
    dice.vel = DiceLaunchVelocity(state);
    dice.acc = sim::Vec2::zero();
}

bool DiceHasSettled(const Ent& dice) {
    return dice.grounded && dice.vel.x.abs() <= sim::ToSimScalar(kDiceSettleSpeed) &&
           dice.vel.y.abs() <= sim::ToSimScalar(kDiceSettleSpeed) && dice.counter_b <= sim::Scalar::zero();
}

void PayCrapsResult(
    Ent& table,
    Ent& dice,
    Ent* prize,
    Ent& player,
    State& state,
    Audio& audio
) {
    (void)audio;
    const int roll = std::clamp(dice.counter_a.trunc_int(), 2, 12);
    if (roll == 7) {
        if (prize != nullptr && prize->active) {
            Ent* const won_prize = SpawnWonPrize(table, *prize, state);
            if (won_prize != nullptr) {
                (void)PlayEntCenterSoundEmitter(state, *won_prize, audio_asset_ids::Present);
            } else {
                UnlockPrize(*prize);
                table.counter_d = sim::Scalar::from_int(2);
                (void)PlayEntCenterSoundEmitter(state, *prize, audio_asset_ids::Present);
            }
        }
        table.counter_c = sim::Scalar::from_int(2);
    } else if (roll > 7) {
        player.money += kCrapsBetAmount * 2U;
        (void)PlayEntCenterSoundEmitter(state, player, audio_asset_ids::CashRegister);
        table.counter_c = sim::Scalar::from_int(1);
    } else {
        (void)PlayEntCenterSoundEmitter(state, table, audio_asset_ids::UiCant);
        table.counter_c = sim::Scalar::from_int(-1);
    }
    table.counter_b = sim::ToSimScalar(kResultPromptFrames);
    SetTableState(table, TableState::Result);
}

void AddCrapsPrompt(Ent& table, State& state, const char* message, std::uint32_t quantity) {
    state.AddWorldPrompt(WorldPrompt{
        .world_pos = sim::ToRenderVec2(table.GetSimCenter() + sim::Vec2::from_pixels(0, -24)),
        .action_text = quantity > 0 ? "RB" : "",
        .message_text = message,
        .show_down_arrow = true,
        .quantity = quantity,
        .icon_anim_id = quantity > 0 ? std::optional<AFrameId>(aframe_ids::GoldIcon)
                                          : std::nullopt,
    });
}

bool PlayerOverlapsTable(const Ent& table, const Ent& player, const Graphics& graphics,
                         const Stage& stage) {
    const sim::AABB table_aabb = table.GetSimAABB();
    const sim::AABB player_aabb = common::GetContactAabbForEnt(player, graphics);
    return WorldAabbsIntersect(stage, table_aabb, player_aabb);
}

bool TryStartCrapsRoll(
    Ent& table,
    Ent& player,
    State& state,
    Audio& audio
) {
    Ent* const dice = GetLinkedEnt(state, table.ent_b);
    if (GetTableState(table) != TableState::Idle ||
        dice == nullptr ||
        !dice->active ||
        IsShopDisturbed(table, state) ||
        !TrySpendMoney(player.vid.id, kCrapsBetAmount, state, audio)) {
        return false;
    }

    LaunchDice(table, *dice, state);
    SetTableState(table, TableState::Rolling);
    (void)PlayEntCenterSoundEmitter(state, table, audio_asset_ids::Throw);
    return true;
}

void StepEntLogicAsCrapsTable(
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

    Ent& table = state.ents.ents[ent_idx];
    if (!table.active || table.type_ != EntType::CrapsTable) {
        return;
    }

    Ent* const dice = GetLinkedEnt(state, table.ent_b);
    Ent* const prize = GetLinkedEnt(state, table.ent_c);
    if (dice != nullptr && dice->active) {
        PrepareCrapsDice(*dice);
    }
    if (prize != nullptr && prize->active && table.counter_d == sim::Scalar::zero()) {
        LockPrize(*prize);
        table.counter_d = sim::Scalar::from_int(1);
    }

    Ent* result_player = nullptr;
    for (const PlayerSlot& slot : state.players.slots) {
        if (!slot.connected || !slot.ent_vid.has_value()) {
            continue;
        }
        Ent* const candidate = state.ents.GetEntMut(*slot.ent_vid);
        if (candidate != nullptr && candidate->active &&
            PlayerOverlapsTable(table, *candidate, graphics, state.stage)) {
            result_player = candidate;
            break;
        }
    }

    if (GetTableState(table) == TableState::Rolling) {
        if (dice != nullptr && dice->active && result_player != nullptr && result_player->active &&
            DiceHasSettled(*dice)) {
            PayCrapsResult(table, *dice, prize, *result_player, state, audio);
        }
    } else if (GetTableState(table) == TableState::Result) {
        table.counter_b -= sim::Scalar::from_int(1);
        if (table.counter_b <= sim::Scalar::zero()) {
            table.counter_b = sim::Scalar::zero();
            table.counter_c = sim::Scalar::zero();
            SetTableState(table, TableState::Idle);
        }
    }

    if (IsShopDisturbed(table, state)) {
        return;
    }

    for (const PlayerSlot& slot : state.players.slots) {
        if (!ShouldSimulatePlayerSlotGameplay(state, slot)) {
            continue;
        }

        Ent* const player = state.ents.GetEntMut(*slot.ent_vid);
        if (player == nullptr || !player->active ||
            !PlayerOverlapsTable(table, *player, graphics, state.stage)) {
            continue;
        }

        state.ClaimInteractForEnt(player->vid);
        if (GetTableState(table) == TableState::Idle) {
            AddCrapsPrompt(table, state, "bet", kCrapsBetAmount);
            if (slot.inputs.equip_button.pressed) {
                (void)world_ops::TryApplyInteractEnt(
                    player->vid,
                    table.vid,
                    state,
                    graphics,
                    audio
                );
                return;
            }
        } else if (GetTableState(table) == TableState::Rolling) {
            AddCrapsPrompt(table, state, "rolling", 0);
        } else if (table.counter_c == sim::Scalar::from_int(2)) {
            AddCrapsPrompt(table, state, "prize", 0);
        } else if (table.counter_c > sim::Scalar::zero()) {
            AddCrapsPrompt(table, state, "win", 0);
        } else if (table.counter_c < sim::Scalar::zero()) {
            AddCrapsPrompt(table, state, "lose", 0);
        }
    }
}

bool OnInteractAsCrapsTable(
    std::size_t ent_idx,
    std::size_t interactor_idx,
    State& state,
    Graphics& graphics,
    Audio& audio
) {
    if (ent_idx >= state.ents.ents.size() ||
        interactor_idx >= state.ents.ents.size()) {
        return false;
    }

    Ent& table = state.ents.ents[ent_idx];
    Ent& player = state.ents.ents[interactor_idx];
    if (!table.active ||
        !player.active ||
        !PlayerOverlapsTable(table, player, graphics, state.stage)) {
        return false;
    }

    return TryStartCrapsRoll(table, player, state, audio);
}

} // namespace

extern const EntSpec kCrapsTableSpec{
    .type_ = EntType::CrapsTable,
    .size = EntSpecSize(16.0F, 16.0F),
    .health = 1,
    .has_physics = false,
    .can_collide = false,
    .can_be_hit = false,
    .can_be_picked_up = false,
    .impassable = false,
    .hurt_on_contact = false,
    .can_be_stomped = false,
    .draw_layer = DrawLayer::Background,
    .render_enabled = false,
    .condition = EntCondition::Normal,
    .display_state = EntDisplayState::Neutral,
    .damage_vuln = DamageVuln::Immune,
    .on_interact = OnInteractAsCrapsTable,
    .step_logic = StepEntLogicAsCrapsTable,
    .aframe_animator = AFrameAnimator::New(aframe_ids::NoSprite),
};

} // namespace splonks::ents::craps_table

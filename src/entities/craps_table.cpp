#include "entities/craps_table.hpp"

#include "audio_emitters.hpp"
#include "buying.hpp"
#include "entities/common/common.hpp"
#include "entity.hpp"
#include "entity/archetype.hpp"
#include "frame_data_animator.hpp"
#include "frame_data_id.hpp"
#include "player_queries.hpp"
#include "state.hpp"
#include "utils.hpp"
#include "world_ops.hpp"
#include "world_query.hpp"

#include <algorithm>
#include <cmath>

namespace splonks::entities::craps_table {

namespace {

constexpr std::uint32_t kCrapsBetAmount = 1000;
constexpr float kDiceRollState = 1.0F;
constexpr float kDiceSettleSpeed = 0.2F;
constexpr float kResultPromptFrames = 120.0F;
constexpr float kPrizeAlphaLocked = 0.55F;

enum class TableState {
    Idle = 0,
    Rolling = 1,
    Result = 2,
};

TableState GetTableState(const Entity& table) {
    return static_cast<TableState>(static_cast<int>(table.counter_a));
}

void SetTableState(Entity& table, TableState state) {
    table.counter_a = static_cast<float>(static_cast<int>(state));
}

Entity* GetLinkedEntity(State& state, std::optional<VID> vid) {
    if (!vid.has_value()) {
        return nullptr;
    }
    return state.entity_manager.GetEntityMut(*vid);
}

bool IsShopDisturbed(const Entity& table, const State& state) {
    const Entity* const shop = table.entity_a.has_value()
        ? state.entity_manager.GetEntity(*table.entity_a)
        : nullptr;
    return shop != nullptr && shop->active && shop->ai_state == EntityAiState::Disturbed;
}

int RollDicePairTotal(State& state) {
    return state.drng.RandomIntInclusive(1, 6) +
           state.drng.RandomIntInclusive(1, 6);
}

void LockPrize(Entity& prize) {
    ClearEntityBuyableState(prize);
    prize.has_physics = false;
    prize.can_collide = false;
    prize.can_be_picked_up = false;
    prize.can_be_hit = false;
    prize.alpha = kPrizeAlphaLocked;
}

void UnlockPrize(Entity& prize) {
    const EntityArchetype& archetype = GetEntityArchetype(prize.type_);
    prize.has_physics = archetype.has_physics;
    prize.can_collide = archetype.can_collide;
    prize.can_be_picked_up = archetype.can_be_picked_up;
    prize.can_be_hit = archetype.can_be_hit;
    prize.alpha = archetype.alpha;
    prize.vel = Vec2::New(0.0F, -2.25F);
    prize.acc = Vec2::New(0.0F, 0.0F);
}

Entity* SpawnWonPrize(Entity& table, const Entity& display_prize, State& state) {
    Entity* const won_prize = world_ops::SpawnEntity(
        state,
        display_prize.type_,
        [&](Entity& entity) {
            ClearEntityBuyableState(entity);
            entity.SetCenter(table.GetCenter() + Vec2::New(0.0F, -18.0F));
            entity.vel = Vec2::New(0.0F, -2.25F);
            entity.acc = Vec2::New(0.0F, 0.0F);
            entity.grounded = false;
        }
    );
    return won_prize;
}

void PrepareCrapsDice(Entity& dice) {
    dice.can_be_picked_up = false;
    dice.can_apply_projectile_contact = false;
    dice.projectile_contact_timer = 0;
    dice.thrown_by.reset();
    dice.held_by_vid.reset();
    dice.attachment_mode = AttachmentMode::None;
}

void LaunchDice(Entity& table, Entity& dice, State& state) {
    PrepareCrapsDice(dice);
    dice.has_physics = true;
    dice.can_collide = true;
    dice.grounded = false;
    dice.counter_a = static_cast<float>(RollDicePairTotal(state));
    dice.counter_b = kDiceRollState;
    dice.rotation = static_cast<float>(state.drng.RandomIntInclusive(0, 359));

    const Vec2 table_center = table.GetCenter();
    dice.SetCenter(table_center + Vec2::New(0.0F, -10.0F));
    dice.vel = Vec2::New(static_cast<float>(state.drng.RandomIntInclusive(-2, 2)), -5.0F);
    dice.acc = Vec2::New(0.0F, 0.0F);
}

bool DiceHasSettled(const Entity& dice) {
    return dice.grounded && std::abs(dice.vel.x) <= kDiceSettleSpeed &&
           std::abs(dice.vel.y) <= kDiceSettleSpeed && dice.counter_b <= 0.0F;
}

void PayCrapsResult(
    Entity& table,
    Entity& dice,
    Entity* prize,
    Entity& player,
    State& state,
    Audio& audio
) {
    (void)audio;
    const int roll = std::clamp(static_cast<int>(std::round(dice.counter_a)), 2, 12);
    if (roll == 7) {
        if (prize != nullptr && prize->active) {
            Entity* const won_prize = SpawnWonPrize(table, *prize, state);
            if (won_prize != nullptr) {
                (void)PlayEntityCenterSoundEmitter(state, *won_prize, audio_asset_ids::Present);
            } else {
                UnlockPrize(*prize);
                table.counter_d = 2.0F;
                (void)PlayEntityCenterSoundEmitter(state, *prize, audio_asset_ids::Present);
            }
        }
        table.counter_c = 2.0F;
    } else if (roll > 7) {
        player.money += kCrapsBetAmount * 2U;
        (void)PlayEntityCenterSoundEmitter(state, player, audio_asset_ids::CashRegister);
        table.counter_c = 1.0F;
    } else {
        (void)PlayEntityCenterSoundEmitter(state, table, audio_asset_ids::UiCant);
        table.counter_c = -1.0F;
    }
    table.counter_b = kResultPromptFrames;
    SetTableState(table, TableState::Result);
}

void AddCrapsPrompt(Entity& table, State& state, const char* message, std::uint32_t quantity) {
    state.AddWorldPrompt(WorldPrompt{
        .world_pos = table.GetCenter() + Vec2::New(0.0F, -24.0F),
        .action_text = quantity > 0 ? "RB" : "",
        .message_text = message,
        .show_down_arrow = true,
        .quantity = quantity,
        .icon_animation_id = quantity > 0 ? std::optional<FrameDataId>(frame_data_ids::GoldIcon)
                                          : std::nullopt,
    });
}

bool PlayerOverlapsTable(const Entity& table, const Entity& player, const Graphics& graphics,
                         const Stage& stage) {
    const AABB table_aabb = table.GetAABB();
    const AABB player_aabb = common::GetContactAabbForEntity(player, graphics);
    return WorldAabbsIntersect(stage, table_aabb, player_aabb);
}

bool TryStartCrapsRoll(
    Entity& table,
    Entity& player,
    State& state,
    Audio& audio
) {
    Entity* const dice = GetLinkedEntity(state, table.entity_b);
    if (GetTableState(table) != TableState::Idle ||
        dice == nullptr ||
        !dice->active ||
        IsShopDisturbed(table, state) ||
        !TrySpendMoney(player.vid.id, kCrapsBetAmount, state, audio)) {
        return false;
    }

    LaunchDice(table, *dice, state);
    SetTableState(table, TableState::Rolling);
    (void)PlayEntityCenterSoundEmitter(state, table, audio_asset_ids::Throw);
    return true;
}

void StepEntityLogicAsCrapsTable(
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
) {
    (void)dt;
    if (entity_idx >= state.entity_manager.entities.size()) {
        return;
    }

    Entity& table = state.entity_manager.entities[entity_idx];
    if (!table.active || table.type_ != EntityType::CrapsTable) {
        return;
    }

    Entity* const dice = GetLinkedEntity(state, table.entity_b);
    Entity* const prize = GetLinkedEntity(state, table.entity_c);
    if (dice != nullptr && dice->active) {
        PrepareCrapsDice(*dice);
    }
    if (prize != nullptr && prize->active && table.counter_d == 0.0F) {
        LockPrize(*prize);
        table.counter_d = 1.0F;
    }

    Entity* result_player = nullptr;
    for (const PlayerSlot& slot : state.players.slots) {
        if (!slot.connected || !slot.entity_vid.has_value()) {
            continue;
        }
        Entity* const candidate = state.entity_manager.GetEntityMut(*slot.entity_vid);
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
        table.counter_b -= 1.0F;
        if (table.counter_b <= 0.0F) {
            table.counter_b = 0.0F;
            table.counter_c = 0.0F;
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

        Entity* const player = state.entity_manager.GetEntityMut(*slot.entity_vid);
        if (player == nullptr || !player->active ||
            !PlayerOverlapsTable(table, *player, graphics, state.stage)) {
            continue;
        }

        state.ClaimInteractForEntity(player->vid);
        if (GetTableState(table) == TableState::Idle) {
            AddCrapsPrompt(table, state, "bet", kCrapsBetAmount);
            if (slot.inputs.equip_button.pressed) {
                (void)world_ops::TryApplyInteractEntity(
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
        } else if (table.counter_c == 2.0F) {
            AddCrapsPrompt(table, state, "prize", 0);
        } else if (table.counter_c > 0.0F) {
            AddCrapsPrompt(table, state, "win", 0);
        } else if (table.counter_c < 0.0F) {
            AddCrapsPrompt(table, state, "lose", 0);
        }
    }
}

bool OnInteractAsCrapsTable(
    std::size_t entity_idx,
    std::size_t interactor_idx,
    State& state,
    Graphics& graphics,
    Audio& audio
) {
    if (entity_idx >= state.entity_manager.entities.size() ||
        interactor_idx >= state.entity_manager.entities.size()) {
        return false;
    }

    Entity& table = state.entity_manager.entities[entity_idx];
    Entity& player = state.entity_manager.entities[interactor_idx];
    if (!table.active ||
        !player.active ||
        !PlayerOverlapsTable(table, player, graphics, state.stage)) {
        return false;
    }

    return TryStartCrapsRoll(table, player, state, audio);
}

} // namespace

extern const EntityArchetype kCrapsTableArchetype{
    .type_ = EntityType::CrapsTable,
    .size = Vec2::New(16.0F, 16.0F),
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
    .condition = EntityCondition::Normal,
    .display_state = EntityDisplayState::Neutral,
    .damage_vulnerability = DamageVulnerability::Immune,
    .on_interact = OnInteractAsCrapsTable,
    .step_logic = StepEntityLogicAsCrapsTable,
    .replica_logic = StepEntityLogicAsCrapsTable,
    .frame_data_animator = FrameDataAnimator::New(frame_data_ids::NoSprite),
};

} // namespace splonks::entities::craps_table

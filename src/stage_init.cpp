#include "stage_init.hpp"

#include "ent/spec.hpp"
#include "player_queries.hpp"
#include "stage_acoustics.hpp"
#include "stage_lighting.hpp"
#include "stage_spawning.hpp"
#include "utils.hpp"

#include <stdexcept>

namespace splonks {

namespace {
unsigned int RandomPercent(DetRng& det_rng) {
    return static_cast<unsigned int>(det_rng.RandomIntInclusive(0, 99));
}

int RandomMoneyType(DetRng& det_rng) {
    return det_rng.RandomIntInclusive(0, 1);
}

void PlacePlayerAtEntrance(State& state) {
    for (unsigned int y = 0; y < state.stage.GetTileHeight(); ++y) {
        for (unsigned int x = 0; x < state.stage.GetTileWidth(); ++x) {
            if (state.stage.GetTile(x, y) != Tile::Entrance) {
                continue;
            }

            const FxVec2 spawn_pos = FxVec2::from_int(
                static_cast<int>(x * kTileSize),
                static_cast<int>(y * kTileSize));
            unsigned int local_player_index = 0;
            for (const PlayerSlot& slot : state.players.slots) {
                if (!ShouldSimulatePlayerSlotGameplay(state, slot)) {
                    continue;
                }
                if (Ent* const player = state.ents.GetEntMut(*slot.ent_vid)) {
                    player->pos =
                        spawn_pos + FxVec2::from_int(static_cast<int>(local_player_index) * 8, 0);
                    player->vel = FxVec2::zero();
                    player->acc = FxVec2::zero();
                }
                ++local_player_index;
            }
            return;
        }
    }

    throw std::runtime_error(
        "No entrance tile found. You have a game breaking bug in the map generation code.");
}

void SpawnConnectedPlayers(State& state, FxVec2 spawn_pos) {
    unsigned int player_index = 0;
    for (const PlayerSlot& slot : state.players.slots) {
        if (!slot.connected || slot.player_id == kInvalidPlayerId) {
            continue;
        }
        const std::optional<VID> player_vid = SpawnPlayerForPlayerId(
            state,
            slot.player_id,
            spawn_pos + FxVec2::from_int(static_cast<int>(player_index) * 8, 0)
        );
        if (player_vid.has_value() &&
            (slot.primary_local || !state.controlled_ent_vid.has_value())) {
            state.controlled_ent_vid = *player_vid;
        }
        ++player_index;
    }
    if (player_index == 0) {
        SpawnPlayer(state, spawn_pos);
    }
}

} // namespace

void InitStage(State& state, bool preserve_player_state) {
    if (state.stage.quest_id == "classic" && state.stage.quest_stage_id == "classic_mines_1") {
        state.depth = 0;
        state.sac_altar_favor = 0;
        state.sac_altar_reward_tier = 0;
    }
    state.respawn_target = state.stage.quest_id.empty()
                               ? StageLoadTarget::ForStageType(state.stage.stage_type)
                               : StageLoadTarget::ForQuestStage(
                                     state.stage.quest_id,
                                     state.stage.quest_stage_id
                                 );
    const StageCarryover carryover =
        preserve_player_state ? CaptureStageCarryover(state) : StageCarryover{};
    InitCommonStageState(state);

    if (!carryover.players.empty()) {
        RestoreStageCarryover(state, carryover);
    } else {
        SpawnConnectedPlayers(state, FxVec2::zero());
    }
    SpawnAuthoredStageEnts(state);

    if (state.stage.quest_id.empty()) {
        // This mirrors the old Rust stage init population pass.
        for (int i = 0; i < 2; ++i) {
            (void)i;
            if (const std::optional<IVec2> random_available_position =
                    state.stage.GetRandomNoncollidablePositionInStage(state.stagegen_drng)) {
                if (const std::optional<VID> vid = state.ents.NewEnt()) {
                    if (Ent* const ent = state.ents.GetEntMut(*vid)) {
                        SetEntAs(*ent, EntType::JetPack);
                        ent->pos = FxVec2::from_int(random_available_position->x, random_available_position->y);
                    }
                }
            }
        }

        for (int i = 0; i < 32; ++i) {
            (void)i;
            if (const std::optional<IVec2> random_available_position =
                    state.stage.GetRandomNoncollidablePositionInStage(state.stagegen_drng)) {
                if (const std::optional<VID> vid = state.ents.NewEnt()) {
                    if (Ent* const money = state.ents.GetEntMut(*vid)) {
                        const EntType money_type =
                            RandomMoneyType(state.stagegen_drng) == 0 ? EntType::Gold : EntType::GoldStack;
                        SetEntAs(*money, money_type);
                        money->pos = FxVec2::from_int(random_available_position->x, random_available_position->y);
                    }
                }
            }
        }

        for (int i = 0; i < 8; ++i) {
            (void)i;
            if (const std::optional<IVec2> random_available_position =
                    state.stage.GetRandomNoncollidablePositionInStage(state.stagegen_drng)) {
                if (const std::optional<VID> vid = state.ents.NewEnt()) {
                    if (Ent* const bat = state.ents.GetEntMut(*vid)) {
                        SetEntAs(*bat, EntType::Bat);
                        bat->pos = FxVec2::from_int(random_available_position->x, random_available_position->y);
                    }
                }
            }
        }

        for (int i = 0; i < 32; ++i) {
            (void)i;
            if (const std::optional<IVec2> random_available_position =
                    state.stage.GetRandomNoncollidablePositionInStage(state.stagegen_drng)) {
                if (const std::optional<VID> vid = state.ents.NewEnt()) {
                    if (Ent* const ent = state.ents.GetEntMut(*vid)) {
                        const unsigned int random_number = RandomPercent(state.stagegen_drng);
                        if (random_number >= 61 && random_number <= 90) {
                            SetEntAs(*ent, EntType::Pot);
                        } else if (random_number >= 91) {
                            SetEntAs(*ent, EntType::Box);
                        } else {
                            SetEntAs(*ent, EntType::Rock);
                        }
                        ent->pos = FxVec2::from_int(random_available_position->x, random_available_position->y);
                    }
                }
            }
        }

        for (int i = 0; i < 32; ++i) {
            (void)i;
            if (const std::optional<IVec2> random_available_position =
                    state.stage.GetRandomNoncollidablePositionInStage(state.stagegen_drng)) {
                if (const std::optional<VID> vid = state.ents.NewEnt()) {
                    if (Ent* const block = state.ents.GetEntMut(*vid)) {
                        SetEntAs(*block, EntType::Block);
                        block->pos = FxVec2::from_int(random_available_position->x, random_available_position->y);
                    }
                }
            }
        }
    }

    PlacePlayerAtEntrance(state);
    if (!carryover.players.empty()) {
        SnapAttachedItemsToPlayer(state);
    }
    InvalidateStageLighting(state);
    InvalidateStageAcoustics(state);
}

} // namespace splonks

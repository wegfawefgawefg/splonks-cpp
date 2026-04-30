#include "stage_init.hpp"

#include "entity/archetype.hpp"
#include "stage_spawning.hpp"

#include <random>
#include <stdexcept>

namespace splonks {

namespace {
unsigned int RandomPercent() {
    static std::random_device device;
    static std::mt19937 generator(device());
    std::uniform_int_distribution<unsigned int> distribution(0, 99);
    return distribution(generator);
}

int RandomMoneyType() {
    static std::random_device device;
    static std::mt19937 generator(device());
    std::uniform_int_distribution<int> distribution(0, 1);
    return distribution(generator);
}

void PlacePlayerAtEntrance(State& state) {
    for (unsigned int y = 0; y < state.stage.GetTileHeight(); ++y) {
        for (unsigned int x = 0; x < state.stage.GetTileWidth(); ++x) {
            if (state.stage.GetTile(x, y) != Tile::Entrance) {
                continue;
            }

            if (state.player_vid.has_value()) {
                if (Entity* const player = state.entity_manager.GetEntityMut(*state.player_vid)) {
                    player->pos = Vec2::New(static_cast<float>(x), static_cast<float>(y)) *
                                  static_cast<float>(kTileSize);
                    player->vel = Vec2::New(0.0F, 0.0F);
                    player->acc = Vec2::New(0.0F, 0.0F);
                }
            }
            return;
        }
    }

    throw std::runtime_error(
        "No entrance tile found. You have a game breaking bug in the map generation code.");
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

    if (carryover.player.has_value()) {
        RestoreStageCarryover(state, carryover);
    } else {
        SpawnPlayer(state, Vec2::New(0.0F, 0.0F));
    }
    SpawnAuthoredStageEntities(state);

    if (state.stage.quest_id.empty()) {
        // This mirrors the old Rust stage init population pass.
        for (int i = 0; i < 2; ++i) {
            (void)i;
            if (const std::optional<IVec2> random_available_position =
                    state.stage.GetRandomNoncollidablePositionInStage()) {
                if (const std::optional<VID> vid = state.entity_manager.NewEntity()) {
                    if (Entity* const entity = state.entity_manager.GetEntityMut(*vid)) {
                        SetEntityAs(*entity, EntityType::JetPack);
                        entity->pos = ToVec2(*random_available_position);
                    }
                }
            }
        }

        for (int i = 0; i < 32; ++i) {
            (void)i;
            if (const std::optional<IVec2> random_available_position =
                    state.stage.GetRandomNoncollidablePositionInStage()) {
                if (const std::optional<VID> vid = state.entity_manager.NewEntity()) {
                    if (Entity* const money = state.entity_manager.GetEntityMut(*vid)) {
                        const EntityType money_type =
                            RandomMoneyType() == 0 ? EntityType::Gold : EntityType::GoldStack;
                        SetEntityAs(*money, money_type);
                        money->pos = ToVec2(*random_available_position);
                    }
                }
            }
        }

        for (int i = 0; i < 8; ++i) {
            (void)i;
            if (const std::optional<IVec2> random_available_position =
                    state.stage.GetRandomNoncollidablePositionInStage()) {
                if (const std::optional<VID> vid = state.entity_manager.NewEntity()) {
                    if (Entity* const bat = state.entity_manager.GetEntityMut(*vid)) {
                        SetEntityAs(*bat, EntityType::Bat);
                        bat->pos = ToVec2(*random_available_position);
                    }
                }
            }
        }

        for (int i = 0; i < 32; ++i) {
            (void)i;
            if (const std::optional<IVec2> random_available_position =
                    state.stage.GetRandomNoncollidablePositionInStage()) {
                if (const std::optional<VID> vid = state.entity_manager.NewEntity()) {
                    if (Entity* const entity = state.entity_manager.GetEntityMut(*vid)) {
                        const unsigned int random_number = RandomPercent();
                        if (random_number >= 61 && random_number <= 90) {
                            SetEntityAs(*entity, EntityType::Pot);
                        } else if (random_number >= 91) {
                            SetEntityAs(*entity, EntityType::Box);
                        } else {
                            SetEntityAs(*entity, EntityType::Rock);
                        }
                        entity->pos = ToVec2(*random_available_position);
                    }
                }
            }
        }

        for (int i = 0; i < 32; ++i) {
            (void)i;
            if (const std::optional<IVec2> random_available_position =
                    state.stage.GetRandomNoncollidablePositionInStage()) {
                if (const std::optional<VID> vid = state.entity_manager.NewEntity()) {
                    if (Entity* const block = state.entity_manager.GetEntityMut(*vid)) {
                        SetEntityAs(*block, EntityType::Block);
                        block->pos = ToVec2(*random_available_position);
                    }
                }
            }
        }
    }

    PlacePlayerAtEntrance(state);
    if (carryover.player.has_value()) {
        SnapAttachedItemsToPlayer(state);
    }
}

} // namespace splonks

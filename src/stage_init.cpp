#include "stage_init.hpp"

#include "entities/bat.hpp"
#include "entities/block.hpp"
#include "entities/breakaway_container.hpp"
#include "entities/jetpack.hpp"
#include "entities/money.hpp"
#include "entities/player.hpp"
#include "entities/rock.hpp"

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

} // namespace

void InitStage(State& state) {
    state.stage_frame = 0;
    state.entity_manager.ClearAllEntities();

    const IVec2 starting_room = state.stage.GetStartingRoom();
    const UVec2 starting_room_tl = ToUVec2(starting_room) * Stage::kRoomShape;
    const UVec2 starting_room_br = starting_room_tl + Stage::kRoomShape;

    if (const std::optional<VID> player_vid = state.entity_manager.NewEntity()) {
        state.player_vid = player_vid;
        if (Entity* const player = state.entity_manager.GetEntityMut(*player_vid)) {
            entities::player::SetEntityPlayer(*player);
        }
    }

    // This mirrors the old Rust stage init population pass.
    for (int i = 0; i < 2; ++i) {
        (void)i;
        if (const std::optional<IVec2> random_available_position =
                state.stage.GetRandomNoncollidablePositionInRandomRoom()) {
            if (const std::optional<VID> vid = state.entity_manager.NewEntity()) {
                if (Entity* const entity = state.entity_manager.GetEntityMut(*vid)) {
                    entities::jetpack::SetEntityJetpack(*entity);
                    entity->pos = ToVec2(*random_available_position);
                }
            }
        }
    }

    for (int i = 0; i < 32; ++i) {
        (void)i;
        if (const std::optional<IVec2> random_available_position =
                state.stage.GetRandomNoncollidablePositionInRandomRoom()) {
            if (const std::optional<VID> vid = state.entity_manager.NewEntity()) {
                if (Entity* const money = state.entity_manager.GetEntityMut(*vid)) {
                    const EntityType money_type =
                        RandomMoneyType() == 0 ? EntityType::Gold : EntityType::GoldStack;
                    entities::money::SetEntityMoney(*money, money_type);
                    money->pos = ToVec2(*random_available_position);
                }
            }
        }
    }

    for (int i = 0; i < 8; ++i) {
        (void)i;
        if (const std::optional<IVec2> random_available_position =
                state.stage.GetRandomNoncollidablePositionInRandomRoom()) {
            if (const std::optional<VID> vid = state.entity_manager.NewEntity()) {
                if (Entity* const bat = state.entity_manager.GetEntityMut(*vid)) {
                    entities::bat::SetEntityBat(*bat);
                    bat->pos = ToVec2(*random_available_position);
                }
            }
        }
    }

    for (int i = 0; i < 32; ++i) {
        (void)i;
        if (const std::optional<IVec2> random_available_position =
                state.stage.GetRandomNoncollidablePositionInRandomRoom()) {
            if (const std::optional<VID> vid = state.entity_manager.NewEntity()) {
                if (Entity* const entity = state.entity_manager.GetEntityMut(*vid)) {
                    const unsigned int random_number = RandomPercent();
                    if (random_number >= 61 && random_number <= 90) {
                        entities::breakaway_container::SetEntityBreakawayContainer(
                            *entity, EntityType::Pot);
                    } else if (random_number >= 91) {
                        entities::breakaway_container::SetEntityBreakawayContainer(
                            *entity, EntityType::Box);
                    } else {
                        entities::rock::SetEntityRock(*entity);
                    }
                    entity->pos = ToVec2(*random_available_position);
                }
            }
        }
    }

    for (int i = 0; i < 32; ++i) {
        (void)i;
        if (const std::optional<IVec2> random_available_position =
                state.stage.GetRandomNoncollidablePositionInRandomRoom()) {
            if (const std::optional<VID> vid = state.entity_manager.NewEntity()) {
                if (Entity* const block = state.entity_manager.GetEntityMut(*vid)) {
                    entities::block::SetEntityBlock(*block);
                    block->pos = ToVec2(*random_available_position);
                }
            }
        }
    }

    bool door_found = false;
    for (unsigned int y = starting_room_tl.y; y < starting_room_br.y && !door_found; ++y) {
        for (unsigned int x = starting_room_tl.x; x < starting_room_br.x; ++x) {
            if (state.stage.GetTile(x, y) == Tile::Entrance) {
                if (state.player_vid) {
                    if (Entity* const player = state.entity_manager.GetEntityMut(*state.player_vid)) {
                        player->pos = Vec2::New(static_cast<float>(x), static_cast<float>(y)) *
                                      static_cast<float>(kTileSize);
                        player->vel = Vec2::New(0.0F, 0.0F);
                    }
                }
                door_found = true;
                break;
            }
        }
    }

    if (!door_found) {
        throw std::runtime_error(
            "No door found in starting room. You have a game breaking bug in the map generation "
            "code. Don't ship with this in.");
    }
}

} // namespace splonks

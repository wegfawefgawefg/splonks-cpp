#include "stage_init.hpp"
#include "tools/tool_archetype.hpp"

#include "entity.hpp"
#include "entities/bat.hpp"
#include "entities/block.hpp"
#include "entities/jetpack.hpp"
#include "entities/money.hpp"
#include "entities/player.hpp"
#include "entities/rock.hpp"
#include "entities/stomp_pad.hpp"
#include "stage_gen/hd_mines.hpp"

#include <algorithm>
#include <random>

#include <stdexcept>

namespace splonks {

namespace {

constexpr int kHangTestStageWidthTiles = 10;
constexpr int kHangTestWallX = 4;
constexpr int kHangTestTopY = 4;
constexpr int kStompTestStageWidthTiles = 10;
constexpr int kStompTestStageHeightTiles = 8;
constexpr int kBorderTestStageWidthTiles = 10;
constexpr int kBorderTestStageHeightTiles = 8;
constexpr Tile kDefaultDebugBorderTile = Tile::CaveDirt;

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

Stage MakeHangTestStage(const HangTestLevelConfig& config) {
    Stage stage;
    stage.stage_type = StageType::Test1;
    const int stage_width = kHangTestStageWidthTiles;
    const int stage_height = std::clamp(config.stage_height_tiles, 16, 512);
    stage.tiles = std::vector<std::vector<Tile>>(
        static_cast<std::size_t>(stage_height),
        std::vector<Tile>(static_cast<std::size_t>(stage_width), Tile::Air)
    );
    stage.rooms = {};
    stage.path = {};
    stage.gravity = 0.3F;
    stage.border = Stage::MakeUniformBorder(kDefaultDebugBorderTile);
    stage.camera_clamp_margin = ToVec2(Stage::kRoomShape * kTileSize) / 2.0F;

    const int wall_x = std::clamp(kHangTestWallX, 1, stage_width - 2);
    const int top_y = std::clamp(kHangTestTopY, 2, stage_height - 8);
    const int cutout_drop_tiles =
        std::clamp(config.cutout_drop_tiles, 2, stage_height - top_y - 4);
    const int cutout_height_tiles =
        std::clamp(config.cutout_height_tiles, 1, stage_height - top_y - cutout_drop_tiles - 1);
    const int cutout_top_y = top_y + cutout_drop_tiles;
    const int cutout_left_x = wall_x;

    for (int y = top_y; y < stage_height; ++y) {
        for (int x = 0; x <= wall_x; ++x) {
            stage.tiles[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] =
                DirtTileForFamilyTile(stage.border.left.tile);
        }
    }

    for (int y = cutout_top_y; y < cutout_top_y + cutout_height_tiles; ++y) {
        stage.tiles[static_cast<std::size_t>(y)][static_cast<std::size_t>(cutout_left_x)] =
            Tile::Air;
    }

    return stage;
}

Stage MakeStompTestStage() {
    Stage stage;
    stage.stage_type = StageType::Test1;
    stage.tiles = std::vector<std::vector<Tile>>(
        static_cast<std::size_t>(kStompTestStageHeightTiles),
        std::vector<Tile>(static_cast<std::size_t>(kStompTestStageWidthTiles), Tile::Air)
    );
    stage.rooms = {};
    stage.path = {};
    stage.gravity = 0.3F;
    stage.border = Stage::MakeUniformBorder(kDefaultDebugBorderTile);
    stage.camera_clamp_margin = ToVec2(Stage::kRoomShape * kTileSize) / 2.0F;

    for (int x = 0; x < kStompTestStageWidthTiles; ++x) {
        stage.tiles[static_cast<std::size_t>(kStompTestStageHeightTiles - 1)][static_cast<std::size_t>(x)] =
            DirtTileForFamilyTile(stage.border.left.tile);
    }

    return stage;
}

StageBorder MakeStageBorderFromDebugConfig(const BorderTestLevelConfig& config) {
    StageBorder border;
    border.left.tile = config.left_tile;
    border.right.tile = config.right_tile;
    border.top.tile = config.top_tile;
    border.bottom.tile = config.bottom_tile;
    border.wrap_x = config.wrap_x;
    border.wrap_y = config.wrap_y;
    border.void_death_y = config.void_death_y;
    return border;
}

Stage MakeBorderTestStage(const BorderTestLevelConfig& config) {
    Stage stage;
    stage.stage_type = StageType::Test1;
    stage.tiles = std::vector<std::vector<Tile>>(
        static_cast<std::size_t>(kBorderTestStageHeightTiles),
        std::vector<Tile>(static_cast<std::size_t>(kBorderTestStageWidthTiles), Tile::Air)
    );
    stage.rooms = {};
    stage.path = {};
    stage.gravity = 0.3F;
    stage.border = MakeStageBorderFromDebugConfig(config);
    stage.camera_clamp_margin = ToVec2(Stage::kRoomShape * kTileSize) / 2.0F;

    for (int x = 0; x < kBorderTestStageWidthTiles; ++x) {
        stage.tiles[static_cast<std::size_t>(kBorderTestStageHeightTiles - 1)]
                   [static_cast<std::size_t>(x)] = Tile::CaveDirt;
    }

    return stage;
}

void InitCommonStageState(State& state) {
    state.stage_frame = 0;
    state.entity_manager.ClearAllEntities();
}

void SpawnPlayer(State& state, const Vec2& pos) {
    if (const std::optional<VID> player_vid = state.entity_manager.NewEntity()) {
        state.player_vid = player_vid;
        state.controlled_entity_vid = player_vid;
        if (Entity* const player = state.entity_manager.GetEntityMut(*player_vid)) {
            SetEntityAs(*player, EntityType::Player);
            player->pos = pos;
            player->vel = Vec2::New(0.0F, 0.0F);

            if (const std::optional<ToolKind> bomb_tool_kind = FindPreferredToolKindForSlotIndex(0)) {
                FillToolSlot(
                    state.EnsureToolSlot(*player_vid, 0),
                    *bomb_tool_kind,
                    static_cast<std::uint16_t>(player->bombs),
                    true
                );
            }

            if (const std::optional<ToolKind> rope_tool_kind = FindPreferredToolKindForSlotIndex(1)) {
                FillToolSlot(
                    state.EnsureToolSlot(*player_vid, 1),
                    *rope_tool_kind,
                    static_cast<std::uint16_t>(player->ropes),
                    true
                );
            }
        }
    }
}

void SpawnAuthoredStageEntities(State& state) {
    for (const StageEntitySpawn& spawn : state.stage.entity_spawns) {
        if (spawn.type_ == EntityType::None) {
            continue;
        }
        const std::optional<VID> vid = state.entity_manager.NewEntity();
        if (!vid) {
            continue;
        }
        Entity* const entity = state.entity_manager.GetEntityMut(*vid);
        if (entity == nullptr) {
            continue;
        }

        SetEntityAs(*entity, spawn.type_);
        entity->pos = spawn.pos;
        entity->facing = spawn.facing;
        entity->vel = Vec2::New(0.0F, 0.0F);
        if (spawn.animation_id != kInvalidFrameDataId) {
            SetAnimation(*entity, spawn.animation_id);
        }
    }
}

void InitHangTestStage(State& state) {
    InitCommonStageState(state);
    state.mouse_trailer_vid.reset();

    const int stage_height = static_cast<int>(state.stage.GetTileHeight());
    const int wall_x = std::clamp(kHangTestWallX, 1, kHangTestStageWidthTiles - 2);
    const int top_y = std::clamp(kHangTestTopY, 2, stage_height - 8);

    const float spawn_x = static_cast<float>((wall_x + 1) * static_cast<int>(kTileSize) - 8);
    const float spawn_y = static_cast<float>(top_y * static_cast<int>(kTileSize) - 14);
    SpawnPlayer(state, Vec2::New(spawn_x, spawn_y));
}

void InitStompTestStage(State& state) {
    InitCommonStageState(state);
    state.mouse_trailer_vid.reset();

    const float player_spawn_x =
        static_cast<float>(4 * static_cast<int>(kTileSize) - 3);
    const float player_spawn_y =
        static_cast<float>(3 * static_cast<int>(kTileSize) - 10);
    SpawnPlayer(state, Vec2::New(player_spawn_x, player_spawn_y));

    if (const std::optional<VID> vid = state.entity_manager.NewEntity()) {
        if (Entity* const stomp_pad = state.entity_manager.GetEntityMut(*vid)) {
            SetEntityAs(*stomp_pad, EntityType::StompPad);
            stomp_pad->pos = Vec2::New(
                static_cast<float>(4 * static_cast<int>(kTileSize)),
                static_cast<float>(4 * static_cast<int>(kTileSize) - 7)
            );
        }
    }
}

void InitBorderTestStage(State& state) {
    InitCommonStageState(state);
    state.mouse_trailer_vid.reset();

    const float player_spawn_x =
        static_cast<float>(4 * static_cast<int>(kTileSize));
    const float player_spawn_y =
        static_cast<float>(5 * static_cast<int>(kTileSize) - 10);
    SpawnPlayer(state, Vec2::New(player_spawn_x, player_spawn_y));
}

} // namespace

void InitStage(State& state) {
    InitCommonStageState(state);

    const IVec2 starting_room = state.stage.GetStartingRoom();
    const auto [starting_room_tl, starting_room_br] =
        state.stage.GetRoomCorners(ToUVec2(starting_room));

    SpawnPlayer(state, Vec2::New(0.0F, 0.0F));
    SpawnAuthoredStageEntities(state);

    if (!stage_gen::hd_mines::UsesHdMinesGenerator(state.stage.stage_type)) {
        // This mirrors the old Rust stage init population pass.
        for (int i = 0; i < 2; ++i) {
            (void)i;
            if (const std::optional<IVec2> random_available_position =
                    state.stage.GetRandomNoncollidablePositionInRandomRoom()) {
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
                    state.stage.GetRandomNoncollidablePositionInRandomRoom()) {
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
                    state.stage.GetRandomNoncollidablePositionInRandomRoom()) {
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
                    state.stage.GetRandomNoncollidablePositionInRandomRoom()) {
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
                    state.stage.GetRandomNoncollidablePositionInRandomRoom()) {
                if (const std::optional<VID> vid = state.entity_manager.NewEntity()) {
                    if (Entity* const block = state.entity_manager.GetEntityMut(*vid)) {
                        SetEntityAs(*block, EntityType::Block);
                        block->pos = ToVec2(*random_available_position);
                    }
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

void InitDebugLevel(State& state) {
    switch (state.debug_level.kind) {
    case DebugLevelKind::Cave1:
        state.stage = Stage::New(StageType::Cave1);
        InitStage(state);
        break;
    case DebugLevelKind::HangTest:
        state.stage = MakeHangTestStage(state.debug_level.hang_test);
        InitHangTestStage(state);
        break;
    case DebugLevelKind::StompTest:
        state.stage = MakeStompTestStage();
        InitStompTestStage(state);
        break;
    case DebugLevelKind::BorderTest:
        state.stage = MakeBorderTestStage(state.debug_level.border_test);
        InitBorderTestStage(state);
        break;
    }
}

} // namespace splonks

#include "debug/debug_stage_builders.hpp"

#include "debug/debug_stage_common.hpp"
#include "ent/spec.hpp"
#include "stage_spawning.hpp"

#include <algorithm>

namespace splonks {

namespace {

constexpr int kHangTestStageWidthTiles = 10;
constexpr int kHangTestWallX = 4;
constexpr int kHangTestTopY = 4;
constexpr int kHangTestMaxDropTiles = 64;
constexpr int kHangTestBottomPaddingTiles = 8;
constexpr int kHangTestMinStageHeightTiles = 16;
constexpr int kStompTestStageWidthTiles = 10;
constexpr int kStompTestStageHeightTiles = 8;
constexpr int kBorderTestStageWidthTiles = 10;
constexpr int kBorderTestStageHeightTiles = 8;
constexpr int kMazeDoorTestStageWidthTiles = 12;
constexpr int kMazeDoorTestStageHeightTiles = 8;

StageBorder MakeStageBorderFromDebugConfig(const BorderTestLevelConfig& config) {
    StageBorder border;
    border.left.tile = config.left_tile;
    border.right.tile = config.right_tile;
    border.top.tile = config.top_tile;
    border.bottom.tile = config.bottom_tile;
    border.void_death_y = config.void_death_y;
    return border;
}

void BuildMazeDoorTestPerimeter(Stage& stage) {
    for (int x = 0; x < kMazeDoorTestStageWidthTiles; ++x) {
        debug_stage::SetTile(stage, x, 0, Tile::CaveDirt);
        debug_stage::SetTile(stage, x, kMazeDoorTestStageHeightTiles - 1, Tile::CaveDirt);
    }
    for (int y = 0; y < kMazeDoorTestStageHeightTiles; ++y) {
        debug_stage::SetTile(stage, 0, y, Tile::CaveDirt);
        debug_stage::SetTile(stage, kMazeDoorTestStageWidthTiles - 1, y, Tile::CaveDirt);
    }
}

FVec2 GetMazeDoorTestPlayerSpawn(MazeDoorTestRoom room) {
    switch (room) {
    case MazeDoorTestRoom::RoomA:
    case MazeDoorTestRoom::RoomB:
    case MazeDoorTestRoom::RoomC:
        return FVec2::New(2.0F * static_cast<float>(kTileSize), 6.0F * static_cast<float>(kTileSize) - 14.0F);
    }
    return FVec2::New(2.0F * static_cast<float>(kTileSize), 6.0F * static_cast<float>(kTileSize) - 14.0F);
}

void SpawnMazeDoorLogo(State& state, const FVec2& pos, const FVec2& vel, MazeDoorTestRoom target_room) {
    const std::optional<VID> vid = state.ents.NewEnt();
    if (!vid) {
        return;
    }
    Ent* const ent = state.ents.GetEntMut(*vid);
    if (ent == nullptr) {
        return;
    }

    SetEntAs(*ent, EntType::DvdLogo);
    ent->pos = ToFxVec2(pos);
    ent->vel = ToFxVec2(vel);
    ent->transition_target = StageTransitionTarget{
        .destination = StageLoadTarget::ForDebugLevel(
            DebugLevelKind::MazeDoorTest,
            static_cast<std::uint8_t>(target_room)
        ),
        .preserve_player_state = true,
    };
}

} // namespace

Stage MakeHangTestStage(const HangTestLevelConfig& config) {
    Stage stage;
    stage.stage_type = StageType::Test1;
    const int stage_width = kHangTestStageWidthTiles;
    const int drop_tiles = std::clamp(config.drop_tiles, 0, kHangTestMaxDropTiles);
    const int stage_height = std::max(
        kHangTestMinStageHeightTiles,
        kHangTestTopY + drop_tiles + kHangTestBottomPaddingTiles
    );
    stage.tiles = std::vector<std::vector<Tile>>(
        static_cast<std::size_t>(stage_height),
        std::vector<Tile>(static_cast<std::size_t>(stage_width), Tile::Air)
    );
    debug_stage::FillBackwall(stage);
    stage.rooms = {};
    stage.path = {};
    stage.border = Stage::MakeUniformBorder(debug_stage::kDefaultBorderTile);
    debug_stage::ApplyDefaultDebugCamera(stage);

    const int wall_x = std::clamp(kHangTestWallX, 1, stage_width - 2);
    const int top_y = std::clamp(kHangTestTopY, 2, stage_height - 8);
    const int right_floor_y = top_y + drop_tiles;

    for (int y = top_y; y < stage_height; ++y) {
        for (int x = 0; x <= wall_x; ++x) {
            stage.tiles[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] =
                debug_stage::kDefaultBorderTile;
        }
    }

    for (int y = right_floor_y; y < stage_height; ++y) {
        for (int x = wall_x + 1; x < stage_width; ++x) {
            stage.tiles[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] =
                debug_stage::kDefaultBorderTile;
        }
    }

    if (drop_tiles >= 4) {
        const int notch_y = top_y + std::max(2, drop_tiles / 2);
        if (notch_y + 1 < right_floor_y) {
            stage.tiles[static_cast<std::size_t>(notch_y)][static_cast<std::size_t>(wall_x)] = Tile::Air;
            stage.tiles[static_cast<std::size_t>(notch_y + 1)][static_cast<std::size_t>(wall_x)] =
                debug_stage::kDefaultBorderTile;
        }
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
    debug_stage::FillBackwall(stage);
    stage.rooms = {};
    stage.path = {};
    stage.border = Stage::MakeUniformBorder(debug_stage::kDefaultBorderTile);
    debug_stage::ApplyDefaultDebugCamera(stage);

    for (int x = 0; x < kStompTestStageWidthTiles; ++x) {
        stage.tiles[static_cast<std::size_t>(kStompTestStageHeightTiles - 1)][static_cast<std::size_t>(x)] =
            debug_stage::kDefaultBorderTile;
    }

    return stage;
}

Stage MakeBorderTestStage(const BorderTestLevelConfig& config) {
    Stage stage;
    stage.stage_type = StageType::Test1;
    stage.tiles = std::vector<std::vector<Tile>>(
        static_cast<std::size_t>(kBorderTestStageHeightTiles),
        std::vector<Tile>(static_cast<std::size_t>(kBorderTestStageWidthTiles), Tile::Air)
    );
    debug_stage::FillBackwall(stage);
    stage.rooms = {};
    stage.path = {};
    stage.border = MakeStageBorderFromDebugConfig(config);
    stage.camera_clamp_margin = ToVec2(Stage::kRoomShape * kTileSize) / 2.0F;
    stage.camera_clamp_enabled = config.camera_clamp_enabled;

    for (int x = 0; x < kBorderTestStageWidthTiles; ++x) {
        stage.tiles[static_cast<std::size_t>(kBorderTestStageHeightTiles - 1)]
                   [static_cast<std::size_t>(x)] = Tile::CaveDirt;
    }

    return stage;
}

Stage MakeMazeDoorTestStage(MazeDoorTestRoom room) {
    Stage stage;
    stage.stage_type = StageType::Test1;
    stage.tiles = std::vector<std::vector<Tile>>(
        static_cast<std::size_t>(kMazeDoorTestStageHeightTiles),
        std::vector<Tile>(static_cast<std::size_t>(kMazeDoorTestStageWidthTiles), Tile::Air)
    );
    debug_stage::FillBackwall(stage);
    stage.rooms = {};
    stage.path = {};
    stage.border = Stage::MakeUniformBorder(debug_stage::kDefaultBorderTile);
    debug_stage::ApplyDefaultDebugCamera(stage);

    BuildMazeDoorTestPerimeter(stage);
    switch (room) {
    case MazeDoorTestRoom::RoomA:
        for (int x = 5; x <= 8; ++x) {
            debug_stage::SetTile(stage, x, 4, Tile::CaveDirt);
        }
        break;
    case MazeDoorTestRoom::RoomB:
        for (int y = 2; y <= 5; ++y) {
            debug_stage::SetTile(stage, 6, y, Tile::CaveDirt);
        }
        debug_stage::SetTile(stage, 3, 4, Tile::CaveDirt);
        debug_stage::SetTile(stage, 8, 3, Tile::CaveDirt);
        break;
    case MazeDoorTestRoom::RoomC:
        for (int x = 3; x <= 5; ++x) {
            debug_stage::SetTile(stage, x, 4, Tile::CaveDirt);
        }
        for (int x = 7; x <= 9; ++x) {
            debug_stage::SetTile(stage, x, 3, Tile::CaveDirt);
        }
        break;
    }

    return stage;
}

void InitHangTestStage(State& state) {
    InitCommonStageState(state);
    state.mouse_trailer_vid.reset();

    const int stage_height = static_cast<int>(state.stage.GetTileHeight());
    const int wall_x = std::clamp(kHangTestWallX, 1, kHangTestStageWidthTiles - 2);
    const int top_y = std::clamp(kHangTestTopY, 2, stage_height - 8);

    const float spawn_x = static_cast<float>((wall_x + 1) * static_cast<int>(kTileSize) - 8);
    const float spawn_y = static_cast<float>(top_y * static_cast<int>(kTileSize) - 14);
    SpawnPlayerAtRenderPosition(state, FVec2::New(spawn_x, spawn_y));
}

void InitStompTestStage(State& state) {
    InitCommonStageState(state);
    state.mouse_trailer_vid.reset();

    const float player_spawn_x = static_cast<float>(4 * static_cast<int>(kTileSize) - 3);
    const float player_spawn_y = static_cast<float>(3 * static_cast<int>(kTileSize) - 10);
    SpawnPlayerAtRenderPosition(state, FVec2::New(player_spawn_x, player_spawn_y));

    if (const std::optional<VID> vid = state.ents.NewEnt()) {
        if (Ent* const stomp_pad = state.ents.GetEntMut(*vid)) {
            SetEntAs(*stomp_pad, EntType::StompPad);
            stomp_pad->pos = ToFxVec2(FVec2::New(
                static_cast<float>(4 * static_cast<int>(kTileSize)),
                static_cast<float>(4 * static_cast<int>(kTileSize) - 7)
            ));
        }
    }
}

void InitBorderTestStage(State& state) {
    InitCommonStageState(state);
    state.mouse_trailer_vid.reset();

    const float player_spawn_x = static_cast<float>(4 * static_cast<int>(kTileSize));
    const float player_spawn_y = static_cast<float>(5 * static_cast<int>(kTileSize) - 10);
    SpawnPlayerAtRenderPosition(state, FVec2::New(player_spawn_x, player_spawn_y));
}

void InitMazeDoorTestStage(State& state, bool preserve_player_state) {
    const StageCarryover carryover =
        preserve_player_state ? CaptureStageCarryover(state) : StageCarryover{};
    InitCommonStageState(state);
    state.mouse_trailer_vid.reset();

    const MazeDoorTestRoom room = state.debug_level.maze_door_test.room;
    const FVec2 spawn_pos = GetMazeDoorTestPlayerSpawn(room);
    if (!carryover.players.empty()) {
        RestoreStageCarryover(state, carryover);
        PlacePlayerAtRenderPosition(state, spawn_pos);
        SnapAttachedItemsToPlayer(state);
    } else {
        SpawnPlayerAtRenderPosition(state, spawn_pos);
    }

    switch (room) {
    case MazeDoorTestRoom::RoomA:
        SpawnMazeDoorLogo(
            state,
            FVec2::New(8.0F * static_cast<float>(kTileSize), 2.0F * static_cast<float>(kTileSize)),
            FVec2::New(-1.0F, 1.0F),
            MazeDoorTestRoom::RoomB
        );
        break;
    case MazeDoorTestRoom::RoomB:
        SpawnMazeDoorLogo(
            state,
            FVec2::New(2.0F * static_cast<float>(kTileSize), 2.0F * static_cast<float>(kTileSize)),
            FVec2::New(1.0F, 1.0F),
            MazeDoorTestRoom::RoomA
        );
        SpawnMazeDoorLogo(
            state,
            FVec2::New(8.0F * static_cast<float>(kTileSize), 2.0F * static_cast<float>(kTileSize)),
            FVec2::New(-1.0F, 1.0F),
            MazeDoorTestRoom::RoomC
        );
        break;
    case MazeDoorTestRoom::RoomC:
        SpawnMazeDoorLogo(
            state,
            FVec2::New(6.0F * static_cast<float>(kTileSize), 2.0F * static_cast<float>(kTileSize)),
            FVec2::New(1.0F, 1.0F),
            MazeDoorTestRoom::RoomA
        );
        break;
    }
}

} // namespace splonks

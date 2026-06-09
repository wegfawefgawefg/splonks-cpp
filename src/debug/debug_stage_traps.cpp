#include "debug/debug_stage_builders.hpp"

#include "debug/debug_stage_common.hpp"
#include "debug/shop_test_stage.hpp"
#include "ent/spec.hpp"
#include "ents/trap_block.hpp"
#include "stage_spawning.hpp"

#include <algorithm>
#include <array>
#include <optional>

namespace splonks {

namespace {

constexpr int kArrowTrapTestStageWidthTiles = 14;
constexpr int kArrowTrapTestStageHeightTiles = 72;
constexpr int kSpikeTestStageWidthTiles = 28;
constexpr int kSpikeTestStageHeightTiles = 16;
constexpr int kTrapDoorTestStageWidthTiles = 112;
constexpr int kTrapDoorTestStageHeightTiles = 10;
constexpr int kCrusherTrapTestStageWidthTiles = 64;
constexpr int kCrusherTrapTestStageHeightTiles = 40;

} // namespace

Stage MakeArrowTrapTestStage() {
    Stage stage;
    stage.stage_type = StageType::Test1;
    stage.tiles = std::vector<std::vector<Tile>>(
        static_cast<std::size_t>(kArrowTrapTestStageHeightTiles),
        std::vector<Tile>(static_cast<std::size_t>(kArrowTrapTestStageWidthTiles), Tile::Air)
    );
    debug_stage::FillBackwall(stage);
    stage.rooms = {};
    stage.path = {};
    stage.border = Stage::MakeUniformBorder(debug_stage::kDefaultBorderTile);
    debug_stage::ApplyDefaultDebugCamera(stage);

    const Tile floor_tile = debug_stage::kDefaultBorderTile;
    for (int y = 0; y < kArrowTrapTestStageHeightTiles; ++y) {
        debug_stage::SetTile(stage, 0, y, floor_tile);
        debug_stage::SetTile(stage, kArrowTrapTestStageWidthTiles - 1, y, floor_tile);
    }
    for (int x = 0; x < kArrowTrapTestStageWidthTiles; ++x) {
        debug_stage::SetTile(stage, x, kArrowTrapTestStageHeightTiles - 1, floor_tile);
    }
    for (int x = 4; x <= 9; ++x) {
        debug_stage::SetTile(stage, x, 4, floor_tile);
    }

    return stage;
}

Stage MakeSpikeTestStage() {
    Stage stage;
    stage.stage_type = StageType::Test1;
    stage.tiles = std::vector<std::vector<Tile>>(
        static_cast<std::size_t>(kSpikeTestStageHeightTiles),
        std::vector<Tile>(static_cast<std::size_t>(kSpikeTestStageWidthTiles), Tile::Air)
    );
    debug_stage::FillBackwall(stage);
    stage.rooms = {};
    stage.path = {};
    stage.border = Stage::MakeUniformBorder(debug_stage::kDefaultBorderTile);
    debug_stage::ApplyDefaultDebugCamera(stage);

    const Tile floor_tile = debug_stage::kDefaultBorderTile;
    constexpr int floor_y = kSpikeTestStageHeightTiles - 1;
    constexpr int spike_y = floor_y - 1;
    for (int x = 0; x < kSpikeTestStageWidthTiles; ++x) {
        debug_stage::SetTile(stage, x, floor_y, floor_tile);
    }

    for (int x = 4; x <= 7; ++x) {
        debug_stage::SetTile(stage, x, spike_y, Tile::Spikes);
    }

    debug_stage::BuildLadder(stage, 11, 5, spike_y - 1);
    debug_stage::SetTile(stage, 11, spike_y, Tile::Spikes);
    debug_stage::SetTile(stage, 10, 10, floor_tile);
    debug_stage::SetTile(stage, 12, 10, floor_tile);

    for (int y = 5; y <= spike_y - 1; ++y) {
        debug_stage::SetTile(stage, 16, y, Tile::Rope);
    }
    debug_stage::SetTile(stage, 16, spike_y, Tile::Spikes);
    debug_stage::SetTile(stage, 15, 10, floor_tile);
    debug_stage::SetTile(stage, 17, 10, floor_tile);

    for (int x = 20; x <= 23; ++x) {
        debug_stage::SetTile(stage, x, 8, floor_tile);
    }
    debug_stage::SetTile(stage, 21, spike_y, Tile::Spikes);
    debug_stage::SetTile(stage, 22, spike_y, Tile::Spikes);

    return stage;
}

Stage MakeTrapDoorTestStage() {
    Stage stage;
    stage.stage_type = StageType::Test1;
    const Tile wall_tile = debug_stage::kDefaultBorderTile;
    stage.tiles = std::vector<std::vector<Tile>>(
        static_cast<std::size_t>(kTrapDoorTestStageHeightTiles),
        std::vector<Tile>(static_cast<std::size_t>(kTrapDoorTestStageWidthTiles), wall_tile)
    );
    debug_stage::FillBackwall(stage);
    stage.rooms = {};
    stage.path = {};
    stage.border = Stage::MakeUniformBorder(debug_stage::kDefaultBorderTile);
    debug_stage::ApplyDefaultDebugCamera(stage);

    constexpr int hallway_top_y = 4;
    constexpr int hallway_bottom_y = 5;
    for (int y = hallway_top_y; y <= hallway_bottom_y; ++y) {
        for (int x = 1; x < kTrapDoorTestStageWidthTiles - 1; ++x) {
            debug_stage::SetTile(stage, x, y, Tile::Air);
        }
    }

    return stage;
}

Stage MakeCrusherTrapTestStage() {
    Stage stage;
    stage.stage_type = StageType::Test1;
    stage.tiles = std::vector<std::vector<Tile>>(
        static_cast<std::size_t>(kCrusherTrapTestStageHeightTiles),
        std::vector<Tile>(static_cast<std::size_t>(kCrusherTrapTestStageWidthTiles), Tile::Air)
    );
    debug_stage::FillBackwall(stage);
    stage.rooms = {};
    stage.path = {};
    stage.border = Stage::MakeUniformBorder(debug_stage::kDefaultBorderTile);
    debug_stage::ApplyDefaultDebugCamera(stage);

    const Tile wall_tile = debug_stage::kDefaultBorderTile;
    for (int x = 0; x < kCrusherTrapTestStageWidthTiles; ++x) {
        debug_stage::SetTile(stage, x, 0, wall_tile);
        debug_stage::SetTile(stage, x, kCrusherTrapTestStageHeightTiles - 1, wall_tile);
    }
    for (int y = 0; y < kCrusherTrapTestStageHeightTiles; ++y) {
        debug_stage::SetTile(stage, 0, y, wall_tile);
        debug_stage::SetTile(stage, kCrusherTrapTestStageWidthTiles - 1, y, wall_tile);
    }

    debug_stage::FillRect(stage, 14, 12, 20, 12, wall_tile);

    debug_stage::FillRect(stage, 3, 12, 6, 12, wall_tile);
    debug_stage::FillRect(stage, 9, 12, 12, 12, wall_tile);
    debug_stage::FillRect(stage, 2, 4, 4, 4, wall_tile);
    debug_stage::FillRect(stage, 11, 4, 13, 4, wall_tile);
    debug_stage::FillRect(stage, 7, 7, 8, 7, wall_tile);
    debug_stage::FillRect(stage, 7, 8, 7, 10, wall_tile);
    debug_stage::FillRect(stage, 2, 34, kCrusherTrapTestStageWidthTiles - 3, 34, wall_tile);

    return stage;
}

void InitArrowTrapTestStage(State& state) {
    InitCommonStageState(state);
    state.mouse_trailer_vid.reset();

    const float player_spawn_x = 6.0F * static_cast<float>(kTileSize);
    const float player_spawn_y = 3.0F * static_cast<float>(kTileSize) - 14.0F;
    SpawnPlayerAtAuthoredPosition(state, FVec2::New(player_spawn_x, player_spawn_y));

    constexpr int kTrapRows = 32;
    constexpr int kFirstTrapY = 6;
    constexpr int kTrapStrideY = 2;
    constexpr int kLeftTrapX = 1;
    constexpr int kRightTrapX = kArrowTrapTestStageWidthTiles - 2;
    for (int i = 0; i < kTrapRows; ++i) {
        const int tile_y = kFirstTrapY + i * kTrapStrideY;
        const FVec2 left_pos = FVec2::New(
            static_cast<float>(kLeftTrapX * static_cast<int>(kTileSize)),
            static_cast<float>(tile_y * static_cast<int>(kTileSize))
        );
        if (const std::optional<VID> trap_vid =
                SpawnStageEntAtAuthoredTopLeft(state, EntType::ArrowTrap, left_pos)) {
            if (Ent* const trap = state.ents.GetEntMut(*trap_vid)) {
                trap->facing = Side::Right;
            }
        }

        const FVec2 right_pos = FVec2::New(
            static_cast<float>(kRightTrapX * static_cast<int>(kTileSize)),
            static_cast<float>(tile_y * static_cast<int>(kTileSize))
        );
        if (const std::optional<VID> trap_vid =
                SpawnStageEntAtAuthoredTopLeft(state, EntType::ArrowTrap, right_pos)) {
            if (Ent* const trap = state.ents.GetEntMut(*trap_vid)) {
                trap->facing = Side::Left;
            }
        }
    }
}

void InitSpikeTestStage(State& state) {
    InitCommonStageState(state);
    state.mouse_trailer_vid.reset();

    SpawnPlayerAtAuthoredPosition(
        state,
        FVec2::New(
            2.0F * static_cast<float>(kTileSize),
            static_cast<float>((kSpikeTestStageHeightTiles - 1) * static_cast<int>(kTileSize)) - 14.0F
        )
    );

    (void)SpawnStageEntAtAuthoredTopLeft(
        state,
        EntType::SpikeShoes,
        FVec2::New(
            25.0F * static_cast<float>(kTileSize),
            static_cast<float>((kSpikeTestStageHeightTiles - 1) * static_cast<int>(kTileSize)) - 16.0F
        )
    );
}

void InitTrapDoorTestStage(State& state) {
    InitCommonStageState(state);
    state.mouse_trailer_vid.reset();

    SpawnPlayerAtAuthoredPosition(
        state,
        FVec2::New(
            4.0F * static_cast<float>(kTileSize),
            6.0F * static_cast<float>(kTileSize) - 14.0F
        )
    );

    constexpr int ceiling_start_y = 2;
    constexpr int floor_y = 6;
    const int closed_top_y =
        floor_y * static_cast<int>(kTileSize) -
        static_cast<int>(ToFloat(GetEntSpec(EntType::Door).size.y));
    constexpr std::array<int, 4> kDropDoorXs{{18, 42, 66, 90}};
    for (const int door_x : kDropDoorXs) {
        if (const std::optional<VID> door_vid = SpawnStageEntAtAuthoredTopLeft(
            state,
            EntType::Door,
            FVec2::New(
                static_cast<float>(door_x * static_cast<int>(kTileSize)),
                static_cast<float>(ceiling_start_y * static_cast<int>(kTileSize))
            )
        )) {
            if (Ent* const door = state.ents.GetEntMut(*door_vid)) {
                door->point_a = IVec2::New(door_x * static_cast<int>(kTileSize), closed_top_y);
                door->point_label_a = PointLabel::Target;
            }
        }
    }

    constexpr std::array<int, 8> kLightXs{{8, 14, 32, 38, 56, 62, 80, 104}};
    for (const int light_x : kLightXs) {
        SpawnShopTestStoreLight(state, light_x, 3);
    }
}

void InitCrusherTrapTestStage(State& state) {
    InitCommonStageState(state);
    state.mouse_trailer_vid.reset();

    SpawnPlayerAtAuthoredPosition(
        state,
        FVec2::New(
            2.0F * static_cast<float>(kTileSize),
            11.0F * static_cast<float>(kTileSize) - 14.0F
        )
    );

    (void)SpawnStageEntAtAuthoredTopLeft(
        state,
        EntType::ThwompTrap,
        FVec2::New(
            5.0F * static_cast<float>(kTileSize),
            1.0F * static_cast<float>(kTileSize)
        )
    );

    const std::array<IVec2, 5> kSquisherBlocks{{
        IVec2::New(2, 8),
        IVec2::New(13, 8),
        IVec2::New(5, 10),
        IVec2::New(10, 10),
        IVec2::New(8, 3),
    }};
    for (const IVec2& tile_pos : kSquisherBlocks) {
        (void)SpawnStageEntAtAuthoredTopLeft(
            state,
            EntType::TrapBlock,
            FVec2::New(
                static_cast<float>(tile_pos.x * static_cast<int>(kTileSize)),
                static_cast<float>(tile_pos.y * static_cast<int>(kTileSize))
            )
        );
    }

    for (int row = 0; row < 5; ++row) {
        const int count = 5 - row;
        for (int column = 0; column < count; ++column) {
            const int tile_x = 19 + row + column;
            if (const std::optional<VID> block_vid = SpawnStageEntAtAuthoredTopLeft(
                state,
                EntType::TrapBlock,
                FVec2::New(
                    static_cast<float>(tile_x * static_cast<int>(kTileSize)),
                    static_cast<float>((7 + row) * static_cast<int>(kTileSize))
                )
            )) {
                if (Ent* const block = state.ents.GetEntMut(*block_vid)) {
                    ents::trap_block::MakeTrapBlockOneShot(*block);
                }
            }
        }
    }

    constexpr int kStressStartX = 2;
    constexpr int kStressStartY = 16;
    constexpr int kStressWidth = kCrusherTrapTestStageWidthTiles - 4;
    constexpr int kStressHeight = 17;
    const int requested_count = std::clamp(
        state.debug_level.crusher_trap_test.stress_squisher_count,
        0,
        static_cast<int>(EntPool::kMaxNumEnts)
    );
    for (int i = 0; i < requested_count; ++i) {
        const int column = i % kStressWidth;
        const int row = i / kStressWidth;
        if (row >= kStressHeight) {
            break;
        }
        const std::optional<VID> block_vid = SpawnStageEntAtAuthoredTopLeft(
            state,
            EntType::TrapBlock,
            FVec2::New(
                static_cast<float>((kStressStartX + column) * static_cast<int>(kTileSize)),
                static_cast<float>((kStressStartY + row) * static_cast<int>(kTileSize))
            )
        );
        if (!block_vid.has_value()) {
            break;
        }
    }
}

} // namespace splonks

#include "debug/debug_stage_builders.hpp"

#include "debug/debug_stage_common.hpp"
#include "effects.hpp"
#include "aframe_id.hpp"
#include "player_queries.hpp"
#include "sim/fxp.hpp"
#include "stage_spawning.hpp"

#include <optional>

namespace splonks {

namespace {

constexpr int kMovingPlatformTestStageWidthTiles = 48;
constexpr int kMovingPlatformTestStageHeightTiles = 18;
constexpr int kAudioTestStageWidthTiles = 96;
constexpr int kAudioTestStageHeightTiles = 24;
constexpr int kParachuteTestStageWidthTiles = 14;
constexpr int kParachuteTestStageHeightTiles = 72;

std::optional<VID> SpawnMovingPlatform(
    State& state,
    const FVec2& pos,
    EntAiState mode,
    const IVec2& point_a,
    const IVec2& point_b,
    float counter_a = 0.0F,
    float counter_b = 1.0F,
    float threshold_a = 0.0F
) {
    const std::optional<VID> vid = SpawnStageEntAtRenderTopLeft(state, EntType::MovingPlatform, pos);
    if (!vid.has_value()) {
        return std::nullopt;
    }

    Ent* const platform = state.ents.GetEntMut(*vid);
    if (platform == nullptr) {
        return std::nullopt;
    }

    platform->ai_state = mode;
    platform->point_a = point_a;
    platform->point_b = point_b;
    platform->counter_a = sim::ToSimScalar(counter_a);
    platform->counter_b = sim::ToSimScalar(counter_b);
    platform->threshold_a = sim::ToSimScalar(threshold_a);
    return vid;
}

} // namespace

Stage MakeMovingPlatformTestStage() {
    Stage stage;
    stage.stage_type = StageType::Test1;
    stage.tiles = std::vector<std::vector<Tile>>(
        static_cast<std::size_t>(kMovingPlatformTestStageHeightTiles),
        std::vector<Tile>(static_cast<std::size_t>(kMovingPlatformTestStageWidthTiles), Tile::Air)
    );
    debug_stage::FillBackwall(stage);
    stage.rooms = {};
    stage.path = {};
    stage.border = Stage::MakeUniformBorder(debug_stage::kDefaultBorderTile);
    debug_stage::ApplyDefaultDebugCamera(stage);

    const int floor_y = kMovingPlatformTestStageHeightTiles - 1;
    const Tile dirt_tile = debug_stage::kDefaultBorderTile;
    const Tile ice_tile = Tile::IceDirt;
    for (int x = 0; x < kMovingPlatformTestStageWidthTiles; ++x) {
        debug_stage::SetTile(stage, x, floor_y, dirt_tile);
    }

    for (int x = 14; x <= 19; ++x) {
        debug_stage::SetTile(stage, x, floor_y, ice_tile);
    }
    for (int x = 31; x <= 36; ++x) {
        debug_stage::SetTile(stage, x, floor_y, ice_tile);
    }

    for (int y = 10; y <= floor_y - 1; ++y) {
        debug_stage::SetTile(stage, 4, y, dirt_tile);
    }
    for (int y = 10; y <= floor_y - 1; ++y) {
        debug_stage::SetTile(stage, 18, y, ice_tile);
    }
    for (int y = 8; y <= floor_y - 1; ++y) {
        debug_stage::SetTile(stage, 33, y, dirt_tile);
    }

    debug_stage::BuildLadder(stage, 10, 9, kMovingPlatformTestStageHeightTiles - 2);
    debug_stage::BuildLadder(stage, 24, 7, kMovingPlatformTestStageHeightTiles - 2);
    debug_stage::BuildLadder(stage, 39, 8, kMovingPlatformTestStageHeightTiles - 2);

    return stage;
}

Stage MakeAudioTestStage() {
    Stage stage;
    stage.stage_type = StageType::Test1;
    stage.tiles = std::vector<std::vector<Tile>>(
        static_cast<std::size_t>(kAudioTestStageHeightTiles),
        std::vector<Tile>(static_cast<std::size_t>(kAudioTestStageWidthTiles), Tile::Air)
    );
    debug_stage::FillBackwall(stage);
    stage.rooms = {};
    stage.path = {};
    stage.border = Stage::MakeUniformBorder(debug_stage::kDefaultBorderTile);
    debug_stage::ApplyDefaultDebugCamera(stage);

    const Tile dirt_tile = debug_stage::kDefaultBorderTile;
    const int floor_y = kAudioTestStageHeightTiles - 1;
    for (int x = 0; x < kAudioTestStageWidthTiles; ++x) {
        debug_stage::SetTile(stage, x, floor_y, dirt_tile);
    }

    debug_stage::FillRect(stage, 0, 4, 28, floor_y - 1, dirt_tile);
    debug_stage::CarveRect(stage, 4, 13, 28, 13);

    debug_stage::FillRect(stage, 67, 3, kAudioTestStageWidthTiles - 1, floor_y - 1, dirt_tile);
    debug_stage::CarveRect(stage, 67, 8, 72, 18);
    debug_stage::CarveRect(stage, 73, 9, 76, 18);
    debug_stage::CarveRect(stage, 77, 10, 80, 18);
    debug_stage::CarveRect(stage, 81, 11, 84, 17);
    debug_stage::CarveRect(stage, 85, 12, 88, 16);
    debug_stage::CarveRect(stage, 89, 13, 91, 15);

    return stage;
}

Stage MakeParachuteTestStage() {
    Stage stage;
    stage.stage_type = StageType::Test1;
    stage.tiles = std::vector<std::vector<Tile>>(
        static_cast<std::size_t>(kParachuteTestStageHeightTiles),
        std::vector<Tile>(static_cast<std::size_t>(kParachuteTestStageWidthTiles), Tile::Air)
    );
    debug_stage::FillBackwall(stage);
    stage.rooms = {};
    stage.path = {};
    stage.border = Stage::MakeUniformBorder(debug_stage::kDefaultBorderTile);
    debug_stage::ApplyDefaultDebugCamera(stage);

    const Tile floor_tile = debug_stage::kDefaultBorderTile;
    for (int x = 0; x < kParachuteTestStageWidthTiles; ++x) {
        debug_stage::SetTile(stage, x, kParachuteTestStageHeightTiles - 1, floor_tile);
    }
    for (int x = 4; x <= 8; ++x) {
        debug_stage::SetTile(stage, x, 5, floor_tile);
    }
    for (int y = 5; y < kParachuteTestStageHeightTiles; ++y) {
        debug_stage::SetTile(stage, 1, y, floor_tile);
    }

    return stage;
}

void InitMovingPlatformTestStage(State& state) {
    InitCommonStageState(state);
    state.mouse_trailer_vid.reset();

    const FVec2 left_platform_pos = FVec2::New(
        6.0F * static_cast<float>(kTileSize),
        8.0F * static_cast<float>(kTileSize)
    );
    const FVec2 middle_platform_pos = FVec2::New(
        23.0F * static_cast<float>(kTileSize),
        10.0F * static_cast<float>(kTileSize)
    );
    const FVec2 circle_center = FVec2::New(
        37.0F * static_cast<float>(kTileSize),
        8.0F * static_cast<float>(kTileSize)
    );
    const float circle_radius = 40.0F;
    const FVec2 right_platform_pos = circle_center + FVec2::New(circle_radius, 0.0F);

    SpawnPlayerAtRenderPosition(state, FVec2::New(left_platform_pos.x + 6.0F, left_platform_pos.y - 14.0F));

    (void)SpawnMovingPlatform(
        state,
        left_platform_pos,
        EntAiState::Idle,
        IVec2::New(6 * static_cast<int>(kTileSize), 8 * static_cast<int>(kTileSize)),
        IVec2::New(14 * static_cast<int>(kTileSize), 8 * static_cast<int>(kTileSize))
    );

    const FVec2 icy_platform_pos = FVec2::New(
        14.0F * static_cast<float>(kTileSize),
        5.0F * static_cast<float>(kTileSize)
    );
    const std::optional<VID> icy_platform_vid = SpawnMovingPlatform(
        state,
        icy_platform_pos,
        EntAiState::Idle,
        IVec2::New(14 * static_cast<int>(kTileSize), 5 * static_cast<int>(kTileSize)),
        IVec2::New(22 * static_cast<int>(kTileSize), 5 * static_cast<int>(kTileSize))
    );
    if (icy_platform_vid.has_value()) {
        if (Ent* const icy_platform = state.ents.GetEntMut(*icy_platform_vid)) {
            icy_platform->size = sim::Vec2::from_pixels(64, 16);
            icy_platform->support_ground_friction = sim::ToSimScalar(1.0F);
            icy_platform->can_be_hung_on = false;
            icy_platform->aframe_animator = AFrameAnimator::New(aframe_ids::IceBlock);
        }
    }

    (void)SpawnMovingPlatform(
        state,
        middle_platform_pos,
        EntAiState::Patrolling,
        IVec2::New(23 * static_cast<int>(kTileSize), 4 * static_cast<int>(kTileSize)),
        IVec2::New(23 * static_cast<int>(kTileSize), 10 * static_cast<int>(kTileSize))
    );

    (void)SpawnMovingPlatform(
        state,
        right_platform_pos,
        EntAiState::Disturbed,
        ToIVec2(circle_center),
        IVec2::New(0, 0),
        0.0F,
        0.0F,
        circle_radius
    );
}

void InitAudioTestStage(State& state) {
    InitCommonStageState(state);
    state.mouse_trailer_vid.reset();

    const float player_spawn_x = 48.0F * static_cast<float>(kTileSize);
    const float player_spawn_y = 23.0F * static_cast<float>(kTileSize) - 14.0F;
    SpawnPlayerAtRenderPosition(state, FVec2::New(player_spawn_x, player_spawn_y));
}

void InitParachuteTestStage(State& state) {
    InitCommonStageState(state);
    state.mouse_trailer_vid.reset();

    const float player_spawn_x = 6.0F * static_cast<float>(kTileSize);
    const float player_spawn_y = 5.0F * static_cast<float>(kTileSize) - 14.0F;
    SpawnPlayerAtRenderPosition(state, FVec2::New(player_spawn_x, player_spawn_y));
    if (Ent* const player = GetPrimaryLocalPlayerMut(state)) {
        (void)AddEffect(*player, EffectId::Parachute, 1);
    }
}

} // namespace splonks

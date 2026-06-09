#include "debug/debug_stage_builders.hpp"

#include "debug/debug_stage_common.hpp"
#include "ent/spec.hpp"
#include "ents/common/common.hpp"
#include "player_queries.hpp"
#include "sim/fxp.hpp"
#include "stage_spawning.hpp"

#include <array>
#include <optional>
#include <utility>

namespace splonks {

namespace {

constexpr int kBowlingTestStageWidthTiles = 80;
constexpr int kBowlingTestStageHeightTiles = 8;
constexpr int kOpposingBodySmackStageWidthTiles = 14;
constexpr int kOpposingBodySmackStageHeightTiles = 8;
constexpr int kMonkeyTestStageWidthTiles = 16;
constexpr int kMonkeyTestStageHeightTiles = 16;
constexpr int kWaterPiranhaTestStageWidthTiles = 28;
constexpr int kWaterPiranhaTestStageHeightTiles = 16;

std::optional<VID> SpawnBowlingCaveman(
    State& state,
    const FVec2& center,
    EntCondition condition,
    const FVec2& vel
) {
    const std::optional<VID> vid = state.ents.NewEnt();
    if (!vid.has_value()) {
        return std::nullopt;
    }
    if (Ent* const caveman = state.ents.GetEntMut(*vid)) {
        SetEntAs(*caveman, EntType::Caveman);
        caveman->SetCenter(ToFxVec2(center));
        caveman->condition = condition;
        caveman->vel = ToFxVec2(vel);
        if (condition == EntCondition::Stunned) {
            caveman->stun_timer = 600;
            TrySetAnim(*caveman, EntDisplayState::Stunned);
        }
    }
    return vid;
}

std::optional<VID> SpawnOpposingBodySmackCaveman(State& state, const FVec2& center, const FVec2& vel) {
    const std::optional<VID> vid = state.ents.NewEnt();
    if (!vid.has_value()) {
        return std::nullopt;
    }

    Ent* const caveman = state.ents.GetEntMut(*vid);
    if (caveman == nullptr) {
        return std::nullopt;
    }

    SetEntAs(*caveman, EntType::Caveman);
    caveman->SetCenter(ToFxVec2(center));
    caveman->condition = EntCondition::Stunned;
    caveman->vel = ToFxVec2(vel);
    caveman->stun_timer = 600;
    caveman->proj_contact_damage_type = DamageType::Attack;
    caveman->proj_contact_damage_amount = 1;
    caveman->proj_contact_timer = 600;
    caveman->max_speed = ToFxScalar(12.0F);
    TrySetAnim(*caveman, EntDisplayState::Stunned);
    return vid;
}

void GiveHeldRockToEnt(State& state, VID holder_vid) {
    Ent* const holder = state.ents.GetEntMut(holder_vid);
    if (holder == nullptr || !holder->active) {
        return;
    }

    const std::optional<VID> rock_vid = state.ents.NewEnt();
    if (!rock_vid.has_value()) {
        return;
    }

    Ent* const rock = state.ents.GetEntMut(*rock_vid);
    if (rock == nullptr) {
        return;
    }

    SetEntAs(*rock, EntType::Rock);
    rock->held_by_vid = holder_vid;
    rock->attach_mode = AttachMode::Held;
    rock->has_physics = false;
    rock->can_collide = false;
    rock->thrown_by.reset();
    rock->thrown_immunity_timer = 0;
    rock->proj_contact_damage_type = DamageType::Attack;
    rock->proj_contact_damage_amount = 1;
    rock->proj_contact_timer = 0;
    rock->vel = sim::FxVec2::zero();
    rock->acc = sim::FxVec2::zero();
    rock->SetCenter(ToFxVec2(ToFVec2(holder->GetCenter()) + FVec2::New(4.0F, 1.0F)));
    holder->holding_vid = rock->vid;
    holder->holding = true;
}

void GiveHeldMattockToEnt(State& state, VID holder_vid) {
    Ent* const holder = state.ents.GetEntMut(holder_vid);
    if (holder == nullptr || !holder->active) {
        return;
    }

    const std::optional<VID> mattock_vid = state.ents.NewEnt();
    if (!mattock_vid.has_value()) {
        return;
    }

    Ent* const mattock = state.ents.GetEntMut(*mattock_vid);
    if (mattock == nullptr) {
        return;
    }

    SetEntAs(*mattock, EntType::Mattock);
    mattock->held_by_vid = holder_vid;
    mattock->attach_mode = AttachMode::Held;
    mattock->has_physics = false;
    mattock->can_collide = false;
    mattock->vel = sim::FxVec2::zero();
    mattock->acc = sim::FxVec2::zero();
    mattock->SetCenter(ToFxVec2(ToFVec2(holder->GetCenter()) + FVec2::New(4.0F, 1.0F)));
    holder->holding_vid = mattock->vid;
    holder->holding = true;
}

} // namespace

Stage MakeBowlingTestStage() {
    Stage stage;
    stage.stage_type = StageType::Test1;
    stage.tiles = std::vector<std::vector<Tile>>(
        static_cast<std::size_t>(kBowlingTestStageHeightTiles),
        std::vector<Tile>(static_cast<std::size_t>(kBowlingTestStageWidthTiles), Tile::Air)
    );
    debug_stage::FillBackwall(stage);
    stage.rooms = {};
    stage.path = {};
    stage.border = Stage::MakeUniformBorder(debug_stage::kDefaultBorderTile);
    debug_stage::ApplyDefaultDebugCamera(stage);

    for (int x = 0; x < kBowlingTestStageWidthTiles; ++x) {
        stage.tiles[static_cast<std::size_t>(kBowlingTestStageHeightTiles - 1)][static_cast<std::size_t>(x)] =
            debug_stage::kDefaultBorderTile;
    }

    return stage;
}

Stage MakeOpposingBodySmackStage() {
    Stage stage;
    stage.stage_type = StageType::Test1;
    stage.tiles = std::vector<std::vector<Tile>>(
        static_cast<std::size_t>(kOpposingBodySmackStageHeightTiles),
        std::vector<Tile>(static_cast<std::size_t>(kOpposingBodySmackStageWidthTiles), Tile::Air)
    );
    debug_stage::FillBackwall(stage);
    stage.rooms = {};
    stage.path = {};
    stage.border = Stage::MakeUniformBorder(debug_stage::kDefaultBorderTile);
    debug_stage::ApplyDefaultDebugCamera(stage);

    for (int x = 0; x < kOpposingBodySmackStageWidthTiles; ++x) {
        stage.tiles[static_cast<std::size_t>(kOpposingBodySmackStageHeightTiles - 1)]
                   [static_cast<std::size_t>(x)] = debug_stage::kDefaultBorderTile;
    }

    return stage;
}

Stage MakeMonkeyTestStage() {
    Stage stage;
    stage.stage_type = StageType::Test1;
    stage.tiles = std::vector<std::vector<Tile>>(
        static_cast<std::size_t>(kMonkeyTestStageHeightTiles),
        std::vector<Tile>(static_cast<std::size_t>(kMonkeyTestStageWidthTiles), Tile::Air)
    );
    debug_stage::FillBackwall(stage);
    stage.rooms = {};
    stage.path = {};
    stage.border = Stage::MakeUniformBorder(debug_stage::kDefaultBorderTile);
    debug_stage::ApplyDefaultDebugCamera(stage);

    const Tile wall_tile = debug_stage::kDefaultBorderTile;
    for (int x = 0; x < kMonkeyTestStageWidthTiles; ++x) {
        debug_stage::SetTile(stage, x, 0, wall_tile);
        debug_stage::SetTile(stage, x, kMonkeyTestStageHeightTiles - 1, wall_tile);
    }
    for (int y = 0; y < kMonkeyTestStageHeightTiles; ++y) {
        debug_stage::SetTile(stage, 0, y, wall_tile);
        debug_stage::SetTile(stage, kMonkeyTestStageWidthTiles - 1, y, wall_tile);
    }

    debug_stage::BuildLadder(stage, 3, 4, 14);
    debug_stage::BuildLadder(stage, 12, 3, 14);
    for (int y = 2; y <= 14; ++y) {
        debug_stage::SetTile(stage, 8, y, Tile::Rope);
    }

    debug_stage::FillRect(stage, 2, 11, 5, 11, wall_tile);
    debug_stage::FillRect(stage, 10, 10, 13, 10, wall_tile);
    debug_stage::FillRect(stage, 5, 7, 7, 7, wall_tile);
    debug_stage::FillRect(stage, 9, 5, 11, 5, wall_tile);
    debug_stage::FillRect(stage, 6, 13, 6, 14, wall_tile);
    debug_stage::FillRect(stage, 10, 13, 10, 14, wall_tile);

    return stage;
}

Stage MakeWaterPiranhaTestStage() {
    Stage stage;
    stage.stage_type = StageType::Test1;
    stage.tiles = std::vector<std::vector<Tile>>(
        static_cast<std::size_t>(kWaterPiranhaTestStageHeightTiles),
        std::vector<Tile>(static_cast<std::size_t>(kWaterPiranhaTestStageWidthTiles), Tile::Air)
    );
    debug_stage::FillBackwall(stage);
    stage.rooms = {};
    stage.path = {};
    stage.border = Stage::MakeUniformBorder(debug_stage::kDefaultBorderTile);
    debug_stage::ApplyDefaultDebugCamera(stage);

    const Tile wall_tile = debug_stage::kDefaultBorderTile;
    for (int x = 0; x < kWaterPiranhaTestStageWidthTiles; ++x) {
        debug_stage::SetTile(stage, x, 0, wall_tile);
        debug_stage::SetTile(stage, x, kWaterPiranhaTestStageHeightTiles - 1, wall_tile);
    }
    for (int y = 0; y < kWaterPiranhaTestStageHeightTiles; ++y) {
        debug_stage::SetTile(stage, 0, y, wall_tile);
        debug_stage::SetTile(stage, kWaterPiranhaTestStageWidthTiles - 1, y, wall_tile);
    }

    debug_stage::FillRect(stage, 1, 13, 26, 14, wall_tile);
    debug_stage::FillRect(stage, 6, 9, 7, 12, wall_tile);
    debug_stage::FillRect(stage, 20, 9, 21, 12, wall_tile);
    debug_stage::SetTile(stage, 5, 12, wall_tile);
    debug_stage::SetTile(stage, 22, 12, wall_tile);
    debug_stage::FillRect(stage, 8, 10, 19, 12, Tile::WaterSwim);
    debug_stage::FillRect(stage, 11, 8, 16, 9, Tile::WaterSwim);
    debug_stage::FillRect(stage, 2, 12, 4, 12, Tile::Air);
    debug_stage::FillRect(stage, 23, 12, 25, 12, Tile::Air);
    debug_stage::FillRect(stage, 12, 5, 15, 5, wall_tile);
    debug_stage::BuildLadder(stage, 3, 9, 12);
    debug_stage::BuildLadder(stage, 24, 9, 12);

    stage.stagegen_annotations.push_back(StageGenAnnotation{
        .world_pos = FVec2::New(8.0F * static_cast<float>(kTileSize), 9.0F * static_cast<float>(kTileSize)),
        .text = "effect-driven water pool: jump, fall timer, damping, props",
    });
    return stage;
}

void InitBowlingTestStage(State& state) {
    InitCommonStageState(state);
    state.mouse_trailer_vid.reset();

    const float player_spawn_x = static_cast<float>(4 * static_cast<int>(kTileSize));
    const float player_spawn_y = static_cast<float>(3 * static_cast<int>(kTileSize) - 10);
    SpawnPlayerAtRenderPosition(state, FVec2::New(player_spawn_x, player_spawn_y));

    const float caveman_center_y =
        static_cast<float>((kBowlingTestStageHeightTiles - 1) * static_cast<int>(kTileSize)) - 8.0F;
    (void)SpawnBowlingCaveman(
        state,
        FVec2::New(8.0F * static_cast<float>(kTileSize), caveman_center_y),
        EntCondition::Stunned,
        FVec2::New(24.0F, 0.0F)
    );
    for (Ent& ent : state.ents.ents) {
        if (!ent.active || ent.type_ != EntType::Caveman ||
            ent.condition != EntCondition::Stunned) {
            continue;
        }
        ent.max_speed = ToFxScalar(24.0F);
        ent.affected_by_ground_friction = false;
        ent.proj_contact_damage_type = DamageType::Attack;
        ent.proj_contact_damage_amount = 1;
        ent.proj_contact_timer = 600;
        break;
    }

    constexpr int kStandingCavemanTileXs[] = {16, 19, 22, 25, 28, 31, 34, 37, 40, 43, 46, 49};
    for (const int tile_x : kStandingCavemanTileXs) {
        const std::optional<VID> caveman_vid = SpawnBowlingCaveman(
            state,
            FVec2::New(static_cast<float>(tile_x * static_cast<int>(kTileSize)), caveman_center_y),
            EntCondition::Normal,
            FVec2::New(0.0F, 0.0F)
        );
        if (caveman_vid.has_value()) {
            GiveHeldRockToEnt(state, *caveman_vid);
        }
    }
}

void InitOpposingBodySmackStage(State& state) {
    InitCommonStageState(state);
    state.mouse_trailer_vid.reset();

    const float player_spawn_x = static_cast<float>(2 * static_cast<int>(kTileSize));
    const float player_spawn_y = static_cast<float>(3 * static_cast<int>(kTileSize) - 10);
    SpawnPlayerAtRenderPosition(state, FVec2::New(player_spawn_x, player_spawn_y));

    const float caveman_center_y =
        static_cast<float>((kOpposingBodySmackStageHeightTiles - 1) * static_cast<int>(kTileSize)) - 8.0F;
    (void)SpawnOpposingBodySmackCaveman(
        state,
        FVec2::New(5.0F * static_cast<float>(kTileSize), caveman_center_y),
        FVec2::New(8.0F, 0.0F)
    );
    (void)SpawnOpposingBodySmackCaveman(
        state,
        FVec2::New(9.0F * static_cast<float>(kTileSize), caveman_center_y),
        FVec2::New(-8.0F, 0.0F)
    );
}

void InitMonkeyTestStage(State& state) {
    InitCommonStageState(state);
    state.mouse_trailer_vid.reset();

    SpawnPlayerAtRenderPosition(
        state,
        FVec2::New(
            2.0F * static_cast<float>(kTileSize),
            14.0F * static_cast<float>(kTileSize) - 14.0F
        )
    );

    const std::array<IVec2, 6> monkey_centers_tiles{{
        IVec2::New(5, 10),
        IVec2::New(11, 9),
        IVec2::New(8, 6),
        IVec2::New(13, 4),
        IVec2::New(4, 13),
        IVec2::New(12, 13),
    }};

    for (std::size_t i = 0; i < monkey_centers_tiles.size(); ++i) {
        const IVec2& tile_pos = monkey_centers_tiles[i];
        if (const std::optional<VID> monkey_vid = SpawnStageEntAtRenderCenter(
            state,
            EntType::Monkey,
            FVec2::New(
                (static_cast<float>(tile_pos.x) + 0.5F) * static_cast<float>(kTileSize),
                (static_cast<float>(tile_pos.y) + 0.5F) * static_cast<float>(kTileSize)
            )
        )) {
            if (Ent* const monkey = state.ents.GetEntMut(*monkey_vid)) {
                monkey->facing = (i % 2 == 0) ? Side::Left : Side::Right;
            }
        }
    }
}

void InitWaterPiranhaTestStage(State& state) {
    InitCommonStageState(state);
    state.mouse_trailer_vid.reset();

    SpawnPlayerAtRenderPosition(
        state,
        FVec2::New(
            3.0F * static_cast<float>(kTileSize),
            13.0F * static_cast<float>(kTileSize) - 14.0F
        )
    );
    if (const std::optional<VID> player_vid = FindPrimaryLocalPlayerVid(state)) {
        GiveHeldMattockToEnt(state, *player_vid);
    }

    const std::array<FVec2, 3> piranha_centers{{
        FVec2::New(10.0F * static_cast<float>(kTileSize), 11.5F * static_cast<float>(kTileSize)),
        FVec2::New(14.0F * static_cast<float>(kTileSize), 10.5F * static_cast<float>(kTileSize)),
        FVec2::New(18.0F * static_cast<float>(kTileSize), 11.5F * static_cast<float>(kTileSize)),
    }};
    for (const FVec2& center : piranha_centers) {
        (void)SpawnStageEntAtRenderCenter(state, EntType::Piranha, center);
    }

    const std::array<std::pair<EntType, FVec2>, 8> props{{
        {EntType::Box, FVec2::New(9.5F, 7.5F) * static_cast<float>(kTileSize)},
        {EntType::Pot, FVec2::New(11.5F, 7.5F) * static_cast<float>(kTileSize)},
        {EntType::Rock, FVec2::New(13.5F, 7.5F) * static_cast<float>(kTileSize)},
        {EntType::Bomb, FVec2::New(15.5F, 7.5F) * static_cast<float>(kTileSize)},
        {EntType::Caveman, FVec2::New(17.5F, 7.5F) * static_cast<float>(kTileSize)},
        {EntType::GoldChunk, FVec2::New(9.5F, 12.5F) * static_cast<float>(kTileSize)},
        {EntType::RubyBig, FVec2::New(12.5F, 12.5F) * static_cast<float>(kTileSize)},
        {EntType::Chest, FVec2::New(18.5F, 12.5F) * static_cast<float>(kTileSize)},
    }};
    for (const auto& [type, center] : props) {
        (void)SpawnStageEntAtRenderCenter(state, type, center);
    }
}

} // namespace splonks

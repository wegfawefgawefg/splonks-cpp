#include "debug/debug_stage_builders.hpp"

#include "debug/debug_stage_common.hpp"
#include "entity/archetype.hpp"
#include "entities/common/common.hpp"
#include "frame_data_id.hpp"
#include "stage_spawning.hpp"
#include "utils.hpp"

#include <array>
#include <optional>

namespace splonks {

namespace {

constexpr int kBoulderTestStageWidthTiles = 40;
constexpr int kBoulderTestStageHeightTiles = 16;
constexpr int kSacAltarTestStageWidthTiles = 96;
constexpr int kSacAltarTestStageHeightTiles = 24;

} // namespace

Stage MakeBoulderTestStage() {
    Stage stage;
    stage.stage_type = StageType::Test1;
    stage.tiles = std::vector<std::vector<Tile>>(
        static_cast<std::size_t>(kBoulderTestStageHeightTiles),
        std::vector<Tile>(static_cast<std::size_t>(kBoulderTestStageWidthTiles), Tile::Air)
    );
    debug_stage::FillBackwall(stage);
    stage.rooms = {};
    stage.path = {};
    stage.border = Stage::MakeUniformBorder(debug_stage::kDefaultBorderTile);
    debug_stage::ApplyDefaultDebugCamera(stage);

    const Tile floor_tile = debug_stage::kDefaultBorderTile;
    for (int x = 0; x < 20; ++x) {
        debug_stage::SetTile(stage, x, 7, floor_tile);
    }
    for (int x = 20; x < kBoulderTestStageWidthTiles; ++x) {
        debug_stage::SetTile(stage, x, kBoulderTestStageHeightTiles - 1, floor_tile);
    }

    for (int x = 8; x <= 11; ++x) {
        debug_stage::SetTile(stage, x, 3, floor_tile);
    }

    for (int y = 5; y <= 6; ++y) {
        debug_stage::SetTile(stage, 12, y, floor_tile);
        debug_stage::SetTile(stage, 13, y, floor_tile);
    }
    debug_stage::SetTile(stage, 16, 6, floor_tile);
    debug_stage::SetTile(stage, 17, 6, floor_tile);

    for (int y = 13; y <= 14; ++y) {
        debug_stage::SetTile(stage, 31, y, floor_tile);
        debug_stage::SetTile(stage, 32, y, floor_tile);
    }
    debug_stage::SetTile(stage, 35, 14, floor_tile);
    debug_stage::SetTile(stage, 36, 14, floor_tile);

    return stage;
}

Stage MakeSacAltarTestStage() {
    Stage stage;
    stage.stage_type = StageType::Test1;
    stage.tiles = std::vector<std::vector<Tile>>(
        static_cast<std::size_t>(kSacAltarTestStageHeightTiles),
        std::vector<Tile>(static_cast<std::size_t>(kSacAltarTestStageWidthTiles), Tile::Air)
    );
    debug_stage::FillBackwall(stage);
    stage.rooms = {};
    stage.path = {};
    stage.border = Stage::MakeUniformBorder(debug_stage::kDefaultBorderTile);
    debug_stage::ApplyDefaultDebugCamera(stage);

    const Tile dirt_tile = debug_stage::kDefaultBorderTile;
    const int floor_y = kSacAltarTestStageHeightTiles - 1;
    for (int x = 0; x < kSacAltarTestStageWidthTiles; ++x) {
        debug_stage::SetTile(stage, x, floor_y, dirt_tile);
    }

    for (int y = 0; y < kSacAltarTestStageHeightTiles; ++y) {
        debug_stage::SetTile(stage, 0, y, dirt_tile);
        debug_stage::SetTile(stage, kSacAltarTestStageWidthTiles - 1, y, dirt_tile);
    }

    return stage;
}

void InitBoulderTestStage(State& state) {
    InitCommonStageState(state);
    state.mouse_trailer_vid.reset();

    const float player_spawn_x = static_cast<float>(9 * static_cast<int>(kTileSize));
    const float player_spawn_y = static_cast<float>(3 * static_cast<int>(kTileSize) - 14);
    SpawnPlayer(state, Vec2::New(player_spawn_x, player_spawn_y));

    const std::optional<VID> altar_left_vid = SpawnStageEntityAtTopLeft(
        state,
        EntityType::Altar,
        Vec2::New(4.0F * static_cast<float>(kTileSize), 6.0F * static_cast<float>(kTileSize))
    );
    if (altar_left_vid.has_value()) {
        if (Entity* const altar_left = state.entity_manager.GetEntityMut(*altar_left_vid)) {
            SetAnimation(*altar_left, frame_data_ids::AltarLeft);
        }
    }

    const std::optional<VID> altar_right_vid = SpawnStageEntityAtTopLeft(
        state,
        EntityType::Altar,
        Vec2::New(5.0F * static_cast<float>(kTileSize), 6.0F * static_cast<float>(kTileSize))
    );
    if (altar_right_vid.has_value()) {
        if (Entity* const altar_right = state.entity_manager.GetEntityMut(*altar_right_vid)) {
            SetAnimation(*altar_right, frame_data_ids::AltarRight);
        }
    }

    const std::optional<VID> giant_tiki_head_vid = SpawnStageEntityAtTopLeft(
        state,
        EntityType::GiantTikiHead,
        Vec2::New(4.0F * static_cast<float>(kTileSize), 1.0F * static_cast<float>(kTileSize))
    );
    const std::optional<VID> idol_vid = SpawnStageEntityAtTopLeft(
        state,
        EntityType::GoldIdol,
        Vec2::New(4.0F * static_cast<float>(kTileSize) + 10.0F, 4.0F * static_cast<float>(kTileSize))
    );
    if (giant_tiki_head_vid.has_value() && idol_vid.has_value()) {
        Entity* const head = state.entity_manager.GetEntityMut(*giant_tiki_head_vid);
        const Entity* const idol = state.entity_manager.GetEntity(*idol_vid);
        if (head != nullptr && idol != nullptr) {
            head->entity_a = *idol_vid;
            head->point_a = ToIVec2(idol->pos);
            head->point_label_a = PointLabel::Target;
        }
    }

    (void)SpawnStageEntityAtCenter(
        state,
        EntityType::Rock,
        Vec2::New(12.0F * static_cast<float>(kTileSize), 7.0F * static_cast<float>(kTileSize) - 8.0F)
    );
    (void)SpawnStageEntityAtCenter(
        state,
        EntityType::GoldChunk,
        Vec2::New(14.0F * static_cast<float>(kTileSize), 7.0F * static_cast<float>(kTileSize) - 8.0F)
    );
    (void)SpawnStageEntityAtCenter(
        state,
        EntityType::EmeraldBig,
        Vec2::New(16.0F * static_cast<float>(kTileSize), 7.0F * static_cast<float>(kTileSize) - 8.0F)
    );
    (void)SpawnStageEntityAtCenter(
        state,
        EntityType::Caveman,
        Vec2::New(18.0F * static_cast<float>(kTileSize), 7.0F * static_cast<float>(kTileSize) - 8.0F)
    );

    (void)SpawnStageEntityAtCenter(
        state,
        EntityType::Rock,
        Vec2::New(31.0F * static_cast<float>(kTileSize), 15.0F * static_cast<float>(kTileSize) - 8.0F)
    );
    (void)SpawnStageEntityAtCenter(
        state,
        EntityType::GoldBars,
        Vec2::New(33.0F * static_cast<float>(kTileSize), 15.0F * static_cast<float>(kTileSize) - 8.0F)
    );
    (void)SpawnStageEntityAtCenter(
        state,
        EntityType::SapphireBig,
        Vec2::New(35.0F * static_cast<float>(kTileSize), 15.0F * static_cast<float>(kTileSize) - 8.0F)
    );
    (void)SpawnStageEntityAtCenter(
        state,
        EntityType::Caveman,
        Vec2::New(37.0F * static_cast<float>(kTileSize), 15.0F * static_cast<float>(kTileSize) - 8.0F)
    );
}

void SpawnSacAltarTestCorpsePile(State& state) {
    constexpr int kCorpseColumns = 10;
    constexpr int kCorpseRows = 5;
    const float floor_center_y =
        static_cast<float>((kSacAltarTestStageHeightTiles - 1) * static_cast<int>(kTileSize)) - 8.0F;
    const float base_x = 28.0F * static_cast<float>(kTileSize);
    const float base_y = floor_center_y - 2.0F;

    for (int row = 0; row < kCorpseRows; ++row) {
        for (int col = 0; col < kCorpseColumns; ++col) {
            const std::optional<VID> caveman_vid = SpawnStageEntityAtCenter(
                state,
                EntityType::Caveman,
                Vec2::New(
                    base_x + static_cast<float>(col) * 9.0F + rng::RandomFloat(-1.25F, 1.25F),
                    base_y - static_cast<float>(row) * 5.5F + rng::RandomFloat(-0.75F, 0.75F)
                )
            );
            if (!caveman_vid.has_value()) {
                continue;
            }
            Entity* const caveman = state.entity_manager.GetEntityMut(*caveman_vid);
            if (caveman == nullptr) {
                continue;
            }
            caveman->health = 0;
            caveman->condition = EntityCondition::Dead;
            caveman->vel = Vec2::New(0.0F, 0.0F);
            caveman->acc = Vec2::New(0.0F, 0.0F);
            TrySetAnimation(*caveman, EntityDisplayState::Dead);
        }
    }
}

void SpawnSacAltarTestIdols(State& state) {
    const float floor_center_y =
        static_cast<float>((kSacAltarTestStageHeightTiles - 1) * static_cast<int>(kTileSize)) - 8.0F;
    constexpr std::array<float, 4> kIdolXTiles{44.0F, 46.5F, 49.0F, 51.5F};
    for (const float x_tile : kIdolXTiles) {
        (void)SpawnStageEntityAtCenter(
            state,
            EntityType::GoldIdol,
            Vec2::New(x_tile * static_cast<float>(kTileSize), floor_center_y)
        );
    }
}

void SpawnSacAltarTestLivingDamsels(State& state) {
    const float floor_center_y =
        static_cast<float>((kSacAltarTestStageHeightTiles - 1) * static_cast<int>(kTileSize)) - 8.0F;
    constexpr std::array<float, 4> kDamselXTiles{16.5F, 18.5F, 20.5F, 22.5F};
    for (const float x_tile : kDamselXTiles) {
        const std::optional<VID> damsel_vid = SpawnStageEntityAtCenter(
            state,
            EntityType::Damsel,
            Vec2::New(x_tile * static_cast<float>(kTileSize), floor_center_y)
        );
        if (!damsel_vid.has_value()) {
            continue;
        }
        Entity* const damsel = state.entity_manager.GetEntityMut(*damsel_vid);
        if (damsel == nullptr) {
            continue;
        }
        damsel->condition = EntityCondition::Stunned;
        damsel->stun_timer = 6000;
        damsel->stun_recovers_on_ground = false;
        damsel->stun_recovers_while_held = false;
        damsel->vel = Vec2::New(0.0F, 0.0F);
        damsel->acc = Vec2::New(0.0F, 0.0F);
        TrySetAnimation(*damsel, EntityDisplayState::Stunned);
    }
}

void SpawnSacAltarTestAltar(State& state, int left_x_tile) {
    const Vec2 altar_left_pos = Vec2::New(
        static_cast<float>(left_x_tile) * static_cast<float>(kTileSize),
        23.0F * static_cast<float>(kTileSize) - 16.0F
    );
    const std::optional<VID> altar_left_vid = SpawnStageEntityAtTopLeft(
        state,
        EntityType::SacAltar,
        altar_left_pos
    );
    const std::optional<VID> altar_right_vid = SpawnStageEntityAtTopLeft(
        state,
        EntityType::SacAltar,
        altar_left_pos + Vec2::New(static_cast<float>(kTileSize), 0.0F)
    );
    const std::optional<VID> altar_topper_vid = SpawnStageEntityAtTopLeft(
        state,
        EntityType::SacAltarTopper,
        altar_left_pos + Vec2::New(0.0F, -static_cast<float>(kTileSize))
    );
    if (altar_left_vid.has_value()) {
        if (Entity* const altar_left = state.entity_manager.GetEntityMut(*altar_left_vid)) {
            SetAnimation(*altar_left, frame_data_ids::SacAltarLeft);
            if (altar_topper_vid.has_value()) {
                altar_left->entity_a = *altar_topper_vid;
            }
        }
    }
    if (altar_right_vid.has_value()) {
        if (Entity* const altar_right = state.entity_manager.GetEntityMut(*altar_right_vid)) {
            SetAnimation(*altar_right, frame_data_ids::SacAltarRight);
            if (altar_left_vid.has_value()) {
                altar_right->entity_a = *altar_left_vid;
            }
        }
    }
    if (altar_topper_vid.has_value()) {
        if (Entity* const topper = state.entity_manager.GetEntityMut(*altar_topper_vid)) {
            SetAnimation(*topper, frame_data_ids::SacAltarTopper);
            if (altar_left_vid.has_value()) {
                topper->entity_a = *altar_left_vid;
            }
        }
    }
}

void InitSacAltarTestStage(State& state) {
    InitCommonStageState(state);
    state.mouse_trailer_vid.reset();

    const float player_spawn_x = 6.0F * static_cast<float>(kTileSize);
    const float player_spawn_y = 23.0F * static_cast<float>(kTileSize) - 14.0F;
    SpawnPlayer(state, Vec2::New(player_spawn_x, player_spawn_y));

    SpawnSacAltarTestAltar(state, 6);
    SpawnSacAltarTestAltar(state, 9);
    SpawnSacAltarTestAltar(state, 12);
    SpawnSacAltarTestLivingDamsels(state);
    SpawnSacAltarTestCorpsePile(state);
    SpawnSacAltarTestIdols(state);
}

} // namespace splonks

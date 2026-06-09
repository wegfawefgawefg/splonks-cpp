#include "debug/debug_stage_builders.hpp"

#include "debug/debug_stage_common.hpp"
#include "ent/spec.hpp"
#include "ents/common/common.hpp"
#include "aframe_id.hpp"
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
    SpawnPlayerAtAuthoredPosition(state, FVec2::New(player_spawn_x, player_spawn_y));

    const std::optional<VID> altar_left_vid = SpawnStageEntAtAuthoredTopLeft(
        state,
        EntType::Altar,
        FVec2::New(4.0F * static_cast<float>(kTileSize), 6.0F * static_cast<float>(kTileSize))
    );
    if (altar_left_vid.has_value()) {
        if (Ent* const altar_left = state.ents.GetEntMut(*altar_left_vid)) {
            SetAnim(*altar_left, aframe_ids::AltarLeft);
        }
    }

    const std::optional<VID> altar_right_vid = SpawnStageEntAtAuthoredTopLeft(
        state,
        EntType::Altar,
        FVec2::New(5.0F * static_cast<float>(kTileSize), 6.0F * static_cast<float>(kTileSize))
    );
    if (altar_right_vid.has_value()) {
        if (Ent* const altar_right = state.ents.GetEntMut(*altar_right_vid)) {
            SetAnim(*altar_right, aframe_ids::AltarRight);
        }
    }

    const std::optional<VID> giant_tiki_head_vid = SpawnStageEntAtAuthoredTopLeft(
        state,
        EntType::GiantTikiHead,
        FVec2::New(4.0F * static_cast<float>(kTileSize), 1.0F * static_cast<float>(kTileSize))
    );
    const std::optional<VID> idol_vid = SpawnStageEntAtAuthoredTopLeft(
        state,
        EntType::GoldIdol,
        FVec2::New(4.0F * static_cast<float>(kTileSize) + 10.0F, 4.0F * static_cast<float>(kTileSize))
    );
    if (giant_tiki_head_vid.has_value() && idol_vid.has_value()) {
        Ent* const head = state.ents.GetEntMut(*giant_tiki_head_vid);
        const Ent* const idol = state.ents.GetEnt(*idol_vid);
        if (head != nullptr && idol != nullptr) {
            head->ent_a = *idol_vid;
            head->point_a = ToPixelIVec2Trunc(idol->pos);
            head->point_label_a = PointLabel::Target;
        }
    }

    (void)SpawnStageEntAtAuthoredCenter(
        state,
        EntType::Rock,
        FVec2::New(12.0F * static_cast<float>(kTileSize), 7.0F * static_cast<float>(kTileSize) - 8.0F)
    );
    (void)SpawnStageEntAtAuthoredCenter(
        state,
        EntType::GoldChunk,
        FVec2::New(14.0F * static_cast<float>(kTileSize), 7.0F * static_cast<float>(kTileSize) - 8.0F)
    );
    (void)SpawnStageEntAtAuthoredCenter(
        state,
        EntType::EmeraldBig,
        FVec2::New(16.0F * static_cast<float>(kTileSize), 7.0F * static_cast<float>(kTileSize) - 8.0F)
    );
    (void)SpawnStageEntAtAuthoredCenter(
        state,
        EntType::Caveman,
        FVec2::New(18.0F * static_cast<float>(kTileSize), 7.0F * static_cast<float>(kTileSize) - 8.0F)
    );

    (void)SpawnStageEntAtAuthoredCenter(
        state,
        EntType::Rock,
        FVec2::New(31.0F * static_cast<float>(kTileSize), 15.0F * static_cast<float>(kTileSize) - 8.0F)
    );
    (void)SpawnStageEntAtAuthoredCenter(
        state,
        EntType::GoldBars,
        FVec2::New(33.0F * static_cast<float>(kTileSize), 15.0F * static_cast<float>(kTileSize) - 8.0F)
    );
    (void)SpawnStageEntAtAuthoredCenter(
        state,
        EntType::SapphireBig,
        FVec2::New(35.0F * static_cast<float>(kTileSize), 15.0F * static_cast<float>(kTileSize) - 8.0F)
    );
    (void)SpawnStageEntAtAuthoredCenter(
        state,
        EntType::Caveman,
        FVec2::New(37.0F * static_cast<float>(kTileSize), 15.0F * static_cast<float>(kTileSize) - 8.0F)
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
            const std::optional<VID> caveman_vid = SpawnStageEntAtAuthoredCenter(
                state,
                EntType::Caveman,
                FVec2::New(
                    base_x + static_cast<float>(col) * 9.0F + rng::RandomFloat(-1.25F, 1.25F),
                    base_y - static_cast<float>(row) * 5.5F + rng::RandomFloat(-0.75F, 0.75F)
                )
            );
            if (!caveman_vid.has_value()) {
                continue;
            }
            Ent* const caveman = state.ents.GetEntMut(*caveman_vid);
            if (caveman == nullptr) {
                continue;
            }
            caveman->health = 0;
            caveman->condition = EntCondition::Dead;
            caveman->vel = FxVec2::zero();
            caveman->acc = FxVec2::zero();
            TrySetAnim(*caveman, EntDisplayState::Dead);
        }
    }
}

void SpawnSacAltarTestIdols(State& state) {
    const float floor_center_y =
        static_cast<float>((kSacAltarTestStageHeightTiles - 1) * static_cast<int>(kTileSize)) - 8.0F;
    constexpr std::array<float, 4> kIdolXTiles{44.0F, 46.5F, 49.0F, 51.5F};
    for (const float x_tile : kIdolXTiles) {
        (void)SpawnStageEntAtAuthoredCenter(
            state,
            EntType::GoldIdol,
            FVec2::New(x_tile * static_cast<float>(kTileSize), floor_center_y)
        );
    }
}

void SpawnSacAltarTestLivingDamsels(State& state) {
    const float floor_center_y =
        static_cast<float>((kSacAltarTestStageHeightTiles - 1) * static_cast<int>(kTileSize)) - 8.0F;
    constexpr std::array<float, 4> kDamselXTiles{16.5F, 18.5F, 20.5F, 22.5F};
    for (const float x_tile : kDamselXTiles) {
        const std::optional<VID> damsel_vid = SpawnStageEntAtAuthoredCenter(
            state,
            EntType::Damsel,
            FVec2::New(x_tile * static_cast<float>(kTileSize), floor_center_y)
        );
        if (!damsel_vid.has_value()) {
            continue;
        }
        Ent* const damsel = state.ents.GetEntMut(*damsel_vid);
        if (damsel == nullptr) {
            continue;
        }
        damsel->condition = EntCondition::Stunned;
        damsel->stun_timer = 6000;
        damsel->stun_recovers_on_ground = false;
        damsel->stun_recovers_while_held = false;
        damsel->vel = FxVec2::zero();
        damsel->acc = FxVec2::zero();
        TrySetAnim(*damsel, EntDisplayState::Stunned);
    }
}

void SpawnSacAltarTestAltar(State& state, int left_x_tile) {
    const FVec2 altar_left_pos = FVec2::New(
        static_cast<float>(left_x_tile) * static_cast<float>(kTileSize),
        23.0F * static_cast<float>(kTileSize) - 16.0F
    );
    const std::optional<VID> altar_left_vid = SpawnStageEntAtAuthoredTopLeft(
        state,
        EntType::SacAltar,
        altar_left_pos
    );
    const std::optional<VID> altar_right_vid = SpawnStageEntAtAuthoredTopLeft(
        state,
        EntType::SacAltar,
        altar_left_pos + FVec2::New(static_cast<float>(kTileSize), 0.0F)
    );
    const std::optional<VID> altar_topper_vid = SpawnStageEntAtAuthoredTopLeft(
        state,
        EntType::SacAltarTopper,
        altar_left_pos + FVec2::New(0.0F, -static_cast<float>(kTileSize))
    );
    if (altar_left_vid.has_value()) {
        if (Ent* const altar_left = state.ents.GetEntMut(*altar_left_vid)) {
            SetAnim(*altar_left, aframe_ids::SacAltarLeft);
            if (altar_topper_vid.has_value()) {
                altar_left->ent_a = *altar_topper_vid;
            }
        }
    }
    if (altar_right_vid.has_value()) {
        if (Ent* const altar_right = state.ents.GetEntMut(*altar_right_vid)) {
            SetAnim(*altar_right, aframe_ids::SacAltarRight);
            if (altar_left_vid.has_value()) {
                altar_right->ent_a = *altar_left_vid;
            }
        }
    }
    if (altar_topper_vid.has_value()) {
        if (Ent* const topper = state.ents.GetEntMut(*altar_topper_vid)) {
            SetAnim(*topper, aframe_ids::SacAltarTopper);
            if (altar_left_vid.has_value()) {
                topper->ent_a = *altar_left_vid;
            }
        }
    }
}

void InitSacAltarTestStage(State& state) {
    InitCommonStageState(state);
    state.mouse_trailer_vid.reset();

    const float player_spawn_x = 6.0F * static_cast<float>(kTileSize);
    const float player_spawn_y = 23.0F * static_cast<float>(kTileSize) - 14.0F;
    SpawnPlayerAtAuthoredPosition(state, FVec2::New(player_spawn_x, player_spawn_y));

    SpawnSacAltarTestAltar(state, 6);
    SpawnSacAltarTestAltar(state, 9);
    SpawnSacAltarTestAltar(state, 12);
    SpawnSacAltarTestLivingDamsels(state);
    SpawnSacAltarTestCorpsePile(state);
    SpawnSacAltarTestIdols(state);
}

} // namespace splonks

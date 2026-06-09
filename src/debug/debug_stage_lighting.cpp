#include "debug/debug_stage_builders.hpp"

#include "debug/debug_stage_common.hpp"
#include "ent/spec.hpp"
#include "player_queries.hpp"
#include "fxp.hpp"
#include "stage_spawning.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace splonks {

namespace {

constexpr int kLightingStressStageWidthTiles = 40;
constexpr int kLightingStressStageHeightTiles = 32;

constexpr std::array<Color3, 8> kDebugLightColors{{
    Color3::New(1.0F, 0.24F, 0.16F),
    Color3::New(1.0F, 0.72F, 0.18F),
    Color3::New(0.36F, 1.0F, 0.22F),
    Color3::New(0.10F, 0.90F, 0.95F),
    Color3::New(0.22F, 0.44F, 1.0F),
    Color3::New(0.72F, 0.28F, 1.0F),
    Color3::New(1.0F, 0.26F, 0.78F),
    Color3::New(0.82F, 1.0F, 0.56F),
}};

void SpawnJetpackOnPlayer(State& state) {
    Ent* const player = GetPrimaryLocalPlayerMut(state);
    if (player == nullptr) {
        return;
    }

    const std::optional<VID> jetpack_vid =
        SpawnStageEntAtRenderCenter(state, EntType::JetPack, ToFVec2(player->GetCenter()));
    if (!jetpack_vid.has_value()) {
        return;
    }

    Ent* const jetpack = state.ents.GetEntMut(*jetpack_vid);
    if (jetpack == nullptr) {
        return;
    }
    player->back_vid = *jetpack_vid;
    jetpack->held_by_vid = player->vid;
    jetpack->attach_mode = AttachMode::Back;
    jetpack->has_physics = false;
    jetpack->can_collide = false;
    jetpack->draw_layer = DrawLayer::Background;
}

void SpawnDebugMovingLight(State& state, int index, int count) {
    constexpr float tile_size = static_cast<float>(kTileSize);
    constexpr float min_x = 3.0F * tile_size;
    constexpr float max_x = static_cast<float>(kLightingStressStageWidthTiles - 4) * tile_size;
    constexpr float min_y = 3.0F * tile_size;
    constexpr float max_y = static_cast<float>(kLightingStressStageHeightTiles - 4) * tile_size;

    const int columns = std::max(1, static_cast<int>(std::ceil(std::sqrt(static_cast<float>(count)))));
    const int rows = std::max(1, (count + columns - 1) / columns);
    const int column = index % columns;
    const int row = index / columns;
    const float u = columns <= 1 ? 0.5F : static_cast<float>(column) / static_cast<float>(columns - 1);
    const float v = rows <= 1 ? 0.5F : static_cast<float>(row) / static_cast<float>(rows - 1);
    const FVec2 home = FVec2::New(std::lerp(min_x, max_x, u), std::lerp(min_y, max_y, v));

    const std::optional<VID> light_vid =
        SpawnStageEntAtRenderCenter(state, EntType::DebugMovingLight, home);
    if (!light_vid.has_value()) {
        return;
    }

    Ent* const light = state.ents.GetEntMut(*light_vid);
    if (light == nullptr) {
        return;
    }

    light->point_a = IVec2::New(static_cast<int>(std::round(home.x)), static_cast<int>(std::round(home.y)));
    light->counter_a = ToFxScalar(static_cast<float>(index) * 0.67F);
    light->counter_b = ToFxScalar(0.018F + (static_cast<float>(index % 7) * 0.003F));
    light->counter_c = ToFxScalar(static_cast<float>(index % 11) * 0.41F);
    light->threshold_a = ToFxScalar(8.0F + static_cast<float>((index * 5) % 22));
    light->threshold_b = ToFxScalar(8.0F + static_cast<float>((index * 7) % 18));
    const std::size_t color_index =
        static_cast<std::size_t>(index) % kDebugLightColors.size();
    light->light_color = ToFxColor3(kDebugLightColors[color_index]);
    light->self_light = ToFxScalar(0.65F);
    light->light_strength = ToFxScalar(0.85F + static_cast<float>(index % 5) * 0.08F);
    light->light_radius = 5 + (index % 5);
}

} // namespace

Stage MakeLightingStressTestStage() {
    Stage stage;
    stage.stage_type = StageType::Test1;
    stage.tiles = std::vector<std::vector<Tile>>(
        static_cast<std::size_t>(kLightingStressStageHeightTiles),
        std::vector<Tile>(static_cast<std::size_t>(kLightingStressStageWidthTiles), Tile::Air)
    );
    debug_stage::FillBackwall(stage);
    stage.rooms = {};
    stage.path = {};
    stage.border = Stage::MakeUniformBorder(debug_stage::kDefaultBorderTile);
    debug_stage::ApplyDefaultDebugCamera(stage);

    const Tile wall_tile = debug_stage::kDefaultBorderTile;
    for (int x = 0; x < kLightingStressStageWidthTiles; ++x) {
        debug_stage::SetTile(stage, x, 0, wall_tile);
        debug_stage::SetTile(stage, x, kLightingStressStageHeightTiles - 1, wall_tile);
    }
    for (int y = 0; y < kLightingStressStageHeightTiles; ++y) {
        debug_stage::SetTile(stage, 0, y, wall_tile);
        debug_stage::SetTile(stage, kLightingStressStageWidthTiles - 1, y, wall_tile);
    }

    debug_stage::FillRect(stage, 18, 17, 22, 17, wall_tile);
    debug_stage::FillRect(stage, 5, 7, 13, 7, wall_tile);
    debug_stage::FillRect(stage, 26, 7, 34, 7, wall_tile);
    debug_stage::FillRect(stage, 5, 24, 13, 24, wall_tile);
    debug_stage::FillRect(stage, 26, 24, 34, 24, wall_tile);
    debug_stage::FillRect(stage, 10, 13, 10, 20, wall_tile);
    debug_stage::FillRect(stage, 29, 13, 29, 20, wall_tile);

    return stage;
}

void InitLightingStressTestStage(State& state) {
    InitCommonStageState(state);
    state.mouse_trailer_vid.reset();

    SpawnPlayerAtRenderPosition(
        state,
        FVec2::New(
            20.0F * static_cast<float>(kTileSize),
            17.0F * static_cast<float>(kTileSize) - 14.0F
        )
    );
    SpawnJetpackOnPlayer(state);

    const int light_count = std::clamp(
        state.debug_level.lighting_stress_test.moving_light_count,
        0,
        static_cast<int>(EntPool::kMaxNumEnts - 8)
    );
    for (int i = 0; i < light_count; ++i) {
        SpawnDebugMovingLight(state, i, light_count);
    }
}

} // namespace splonks

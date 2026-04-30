#include "debug/debug_stage_builders.hpp"

#include "debug/debug_stage_common.hpp"
#include "entity/archetype.hpp"
#include "entities/common/common.hpp"
#include "stage_spawning.hpp"

#include <optional>

namespace splonks {

namespace {

constexpr int kBowlingTestStageWidthTiles = 80;
constexpr int kBowlingTestStageHeightTiles = 8;
constexpr int kOpposingBodySmackStageWidthTiles = 14;
constexpr int kOpposingBodySmackStageHeightTiles = 8;

std::optional<VID> SpawnBowlingCaveman(
    State& state,
    const Vec2& center,
    EntityCondition condition,
    const Vec2& vel
) {
    const std::optional<VID> vid = state.entity_manager.NewEntity();
    if (!vid.has_value()) {
        return std::nullopt;
    }
    if (Entity* const caveman = state.entity_manager.GetEntityMut(*vid)) {
        SetEntityAs(*caveman, EntityType::Caveman);
        caveman->SetCenter(center);
        caveman->condition = condition;
        caveman->vel = vel;
        if (condition == EntityCondition::Stunned) {
            caveman->stun_timer = 600;
            TrySetAnimation(*caveman, EntityDisplayState::Stunned);
        }
    }
    return vid;
}

std::optional<VID> SpawnOpposingBodySmackCaveman(State& state, const Vec2& center, const Vec2& vel) {
    const std::optional<VID> vid = state.entity_manager.NewEntity();
    if (!vid.has_value()) {
        return std::nullopt;
    }

    Entity* const caveman = state.entity_manager.GetEntityMut(*vid);
    if (caveman == nullptr) {
        return std::nullopt;
    }

    SetEntityAs(*caveman, EntityType::Caveman);
    caveman->SetCenter(center);
    caveman->condition = EntityCondition::Stunned;
    caveman->vel = vel;
    caveman->stun_timer = 600;
    caveman->projectile_contact_damage_type = DamageType::Attack;
    caveman->projectile_contact_damage_amount = 1;
    caveman->projectile_contact_timer = 600;
    caveman->max_speed = 12.0F;
    TrySetAnimation(*caveman, EntityDisplayState::Stunned);
    return vid;
}

void GiveHeldRockToEntity(State& state, VID holder_vid) {
    Entity* const holder = state.entity_manager.GetEntityMut(holder_vid);
    if (holder == nullptr || !holder->active) {
        return;
    }

    const std::optional<VID> rock_vid = state.entity_manager.NewEntity();
    if (!rock_vid.has_value()) {
        return;
    }

    Entity* const rock = state.entity_manager.GetEntityMut(*rock_vid);
    if (rock == nullptr) {
        return;
    }

    SetEntityAs(*rock, EntityType::Rock);
    rock->held_by_vid = holder_vid;
    rock->attachment_mode = AttachmentMode::Held;
    rock->has_physics = false;
    rock->can_collide = false;
    rock->thrown_by.reset();
    rock->thrown_immunity_timer = 0;
    rock->projectile_contact_damage_type = DamageType::Attack;
    rock->projectile_contact_damage_amount = 1;
    rock->projectile_contact_timer = 0;
    rock->vel = Vec2::New(0.0F, 0.0F);
    rock->acc = Vec2::New(0.0F, 0.0F);
    rock->SetCenter(holder->GetCenter() + Vec2::New(4.0F, 1.0F));
    holder->holding_vid = rock->vid;
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

void InitBowlingTestStage(State& state) {
    InitCommonStageState(state);
    state.mouse_trailer_vid.reset();

    const float player_spawn_x = static_cast<float>(4 * static_cast<int>(kTileSize));
    const float player_spawn_y = static_cast<float>(3 * static_cast<int>(kTileSize) - 10);
    SpawnPlayer(state, Vec2::New(player_spawn_x, player_spawn_y));

    const float caveman_center_y =
        static_cast<float>((kBowlingTestStageHeightTiles - 1) * static_cast<int>(kTileSize)) - 8.0F;
    (void)SpawnBowlingCaveman(
        state,
        Vec2::New(8.0F * static_cast<float>(kTileSize), caveman_center_y),
        EntityCondition::Stunned,
        Vec2::New(24.0F, 0.0F)
    );
    for (Entity& entity : state.entity_manager.entities) {
        if (!entity.active || entity.type_ != EntityType::Caveman ||
            entity.condition != EntityCondition::Stunned) {
            continue;
        }
        entity.max_speed = 24.0F;
        entity.affected_by_ground_friction = false;
        entity.projectile_contact_damage_type = DamageType::Attack;
        entity.projectile_contact_damage_amount = 1;
        entity.projectile_contact_timer = 600;
        break;
    }

    constexpr int kStandingCavemanTileXs[] = {16, 19, 22, 25, 28, 31, 34, 37, 40, 43, 46, 49};
    for (const int tile_x : kStandingCavemanTileXs) {
        const std::optional<VID> caveman_vid = SpawnBowlingCaveman(
            state,
            Vec2::New(static_cast<float>(tile_x * static_cast<int>(kTileSize)), caveman_center_y),
            EntityCondition::Normal,
            Vec2::New(0.0F, 0.0F)
        );
        if (caveman_vid.has_value()) {
            GiveHeldRockToEntity(state, *caveman_vid);
        }
    }
}

void InitOpposingBodySmackStage(State& state) {
    InitCommonStageState(state);
    state.mouse_trailer_vid.reset();

    const float player_spawn_x = static_cast<float>(2 * static_cast<int>(kTileSize));
    const float player_spawn_y = static_cast<float>(3 * static_cast<int>(kTileSize) - 10);
    SpawnPlayer(state, Vec2::New(player_spawn_x, player_spawn_y));

    const float caveman_center_y =
        static_cast<float>((kOpposingBodySmackStageHeightTiles - 1) * static_cast<int>(kTileSize)) - 8.0F;
    (void)SpawnOpposingBodySmackCaveman(
        state,
        Vec2::New(5.0F * static_cast<float>(kTileSize), caveman_center_y),
        Vec2::New(8.0F, 0.0F)
    );
    (void)SpawnOpposingBodySmackCaveman(
        state,
        Vec2::New(9.0F * static_cast<float>(kTileSize), caveman_center_y),
        Vec2::New(-8.0F, 0.0F)
    );
}

} // namespace splonks

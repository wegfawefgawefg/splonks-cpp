#include "stage_init.hpp"
#include "tools/tool_archetype.hpp"

#include "buying.hpp"
#include "entity.hpp"
#include "entities/common/common.hpp"
#include "entities/bat.hpp"
#include "entities/block.hpp"
#include "entities/jetpack.hpp"
#include "entities/money.hpp"
#include "entities/player.hpp"
#include "entities/rock.hpp"
#include "entities/shop.hpp"
#include "entities/store_light.hpp"
#include "entities/damsel.hpp"
#include "entities/stomp_pad.hpp"
#include "stage_gen/splk_mines.hpp"
#include "tile_archetype.hpp"

#include <algorithm>
#include <array>
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
constexpr int kMazeDoorTestStageWidthTiles = 12;
constexpr int kMazeDoorTestStageHeightTiles = 8;
constexpr int kBowlingTestStageWidthTiles = 80;
constexpr int kBowlingTestStageHeightTiles = 8;
constexpr int kOpposingBodySmackStageWidthTiles = 14;
constexpr int kOpposingBodySmackStageHeightTiles = 8;
constexpr int kShopTestStageWidthTiles = 80;
constexpr int kShopTestStageHeightTiles = 12;
constexpr Tile kDefaultDebugBorderTile = Tile::CaveDirt;
constexpr std::array<Tile, 3> kCaveBackwallFillTiles{{
    Tile::CaveAir0,
    Tile::CaveAir1,
    Tile::CaveAir2,
}};

struct StageCarryover {
    std::optional<Entity> player;
    std::optional<Entity> held_item;
    std::optional<Entity> back_item;
    std::optional<EntityToolState> player_tools;
};

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

void FillDebugStageBackwall(Stage& stage) {
    stage.FillBackwall(std::vector<Tile>(kCaveBackwallFillTiles.begin(), kCaveBackwallFillTiles.end()));
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
    FillDebugStageBackwall(stage);
    stage.rooms = {};
    stage.path = {};
    stage.gravity = 0.3F;
    stage.border = Stage::MakeUniformBorder(kDefaultDebugBorderTile);
    stage.camera_clamp_margin = ToVec2(Stage::kRoomShape * kTileSize) / 2.0F;
    stage.camera_clamp_enabled = true;

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
    FillDebugStageBackwall(stage);
    stage.rooms = {};
    stage.path = {};
    stage.gravity = 0.3F;
    stage.border = Stage::MakeUniformBorder(kDefaultDebugBorderTile);
    stage.camera_clamp_margin = ToVec2(Stage::kRoomShape * kTileSize) / 2.0F;
    stage.camera_clamp_enabled = true;

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
    FillDebugStageBackwall(stage);
    stage.rooms = {};
    stage.path = {};
    stage.gravity = 0.3F;
    stage.border = MakeStageBorderFromDebugConfig(config);
    stage.camera_clamp_margin = ToVec2(Stage::kRoomShape * kTileSize) / 2.0F;
    stage.camera_clamp_enabled = config.camera_clamp_enabled;

    for (int x = 0; x < kBorderTestStageWidthTiles; ++x) {
        stage.tiles[static_cast<std::size_t>(kBorderTestStageHeightTiles - 1)]
                   [static_cast<std::size_t>(x)] = Tile::CaveDirt;
    }

    return stage;
}


void SetStageTile(Stage& stage, int x, int y, Tile tile) {
    if (x < 0 || y < 0 || x >= static_cast<int>(stage.GetTileWidth()) ||
        y >= static_cast<int>(stage.GetTileHeight())) {
        return;
    }
    stage.tiles[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = tile;
}

void SetStageBackwallTile(Stage& stage, int x, int y, Tile tile) {
    if (x < 0 || y < 0 || x >= static_cast<int>(stage.GetTileWidth()) ||
        y >= static_cast<int>(stage.GetTileHeight())) {
        return;
    }
    stage.SetBackwallTile(IVec2::New(x, y), tile);
}

struct ShopTestStallSpec {
    int left_x = 0;
    int right_x = 0;
};

struct ShopTestItemSpec {
    EntityType type_ = EntityType::None;
    int tile_x = 0;
    std::uint32_t price = 0;
};

std::optional<VID> SpawnStageEntityAtTopLeft(State& state, EntityType type_, const Vec2& pos);

std::optional<Vec2> FindStoreLightTopLeftFromAnchor(
    const Stage& stage,
    int anchor_tile_x,
    int start_tile_y
) {
    if (anchor_tile_x < 0 || anchor_tile_x >= static_cast<int>(stage.GetTileWidth())) {
        return std::nullopt;
    }

    for (int y = start_tile_y; y >= 0; --y) {
        if (!GetTileArchetype(stage.GetTile(
                static_cast<unsigned int>(anchor_tile_x),
                static_cast<unsigned int>(y)
            )).solid) {
            continue;
        }

        const int light_tile_y = y + 1;
        if (light_tile_y >= static_cast<int>(stage.GetTileHeight())) {
            return std::nullopt;
        }

        return Vec2::New(
            static_cast<float>(anchor_tile_x * static_cast<int>(kTileSize)),
            static_cast<float>(light_tile_y * static_cast<int>(kTileSize))
        );
    }

    return std::nullopt;
}

void BuildShopTestStall(Stage& stage, const ShopTestStallSpec& stall) {
    constexpr int kShopTopY = 4;
    constexpr int kShopBottomY = 10;

    SetStageTile(stage, stall.left_x, kShopTopY, Tile::LawsonLeftTopper);
    for (int x = stall.left_x + 1; x < stall.right_x; ++x) {
        SetStageTile(stage, x, kShopTopY, Tile::LawsonMiddleTopper);
    }
    SetStageTile(stage, stall.right_x, kShopTopY, Tile::LawsonRightTopper);

    for (int y = kShopTopY + 1; y <= kShopBottomY - 2; ++y) {
        SetStageTile(stage, stall.left_x, y, Tile::LawsonWall);
        SetStageTile(stage, stall.right_x, y, Tile::LawsonWall);
    }
    for (int y = kShopTopY + 1; y <= kShopBottomY; ++y) {
        for (int x = stall.left_x + 1; x < stall.right_x; ++x) {
            SetStageBackwallTile(stage, x, y, Tile::LawsonInside);
        }
    }

    SetStageBackwallTile(stage, stall.left_x, kShopBottomY - 1, Tile::LawsonWall);
    SetStageBackwallTile(stage, stall.left_x, kShopBottomY, Tile::LawsonWall);
    SetStageBackwallTile(stage, stall.right_x, kShopBottomY - 1, Tile::LawsonWall);
    SetStageBackwallTile(stage, stall.right_x, kShopBottomY, Tile::LawsonWall);

    SetStageTile(stage, stall.left_x, kShopBottomY - 1, Tile::Air);
    SetStageTile(stage, stall.left_x, kShopBottomY, Tile::Air);

    const int floor_y = static_cast<int>(stage.GetTileHeight()) - 1;
    for (int x = stall.left_x; x <= stall.right_x; ++x) {
        SetStageTile(stage, x, floor_y, Tile::LawsonFloor);
    }
}

AABB MakeShopTestArea(const ShopTestStallSpec& stall) {
    constexpr int kShopTopY = 4;
    constexpr int kShopBottomY = 10;
    return AABB::New(
        Vec2::New(
            static_cast<float>(stall.left_x * static_cast<int>(kTileSize)),
            static_cast<float>(kShopTopY * static_cast<int>(kTileSize))
        ),
        Vec2::New(
            static_cast<float>((stall.right_x + 1) * static_cast<int>(kTileSize) - 1),
            static_cast<float>((kShopBottomY + 1) * static_cast<int>(kTileSize) - 1)
        )
    );
}

std::optional<VID> SpawnShopTestShop(
    State& state,
    const ShopTestStallSpec& stall,
    int shopkeeper_tile_x
) {
    const AABB shop_area = MakeShopTestArea(stall);
    const std::optional<VID> shop_vid =
        SpawnStageEntityAtTopLeft(state, EntityType::Shop, shop_area.tl);
    if (!shop_vid.has_value()) {
        return std::nullopt;
    }

    Entity* const shop = state.entity_manager.GetEntityMut(*shop_vid);
    if (shop == nullptr) {
        return std::nullopt;
    }
    entities::shop::SetShopArea(*shop, shop_area);

    const std::optional<VID> shopkeeper_vid = SpawnStageEntityAtTopLeft(
        state,
        EntityType::Shopkeeper,
        Vec2::New(
            static_cast<float>(shopkeeper_tile_x * static_cast<int>(kTileSize)),
            9.0F * static_cast<float>(kTileSize) - 16.0F
        )
    );
    if (shopkeeper_vid.has_value()) {
        shop->entity_a = *shopkeeper_vid;
        if (Entity* const shopkeeper = state.entity_manager.GetEntityMut(*shopkeeper_vid)) {
            shopkeeper->entity_a = *shop_vid;
        }
    }

    return shop_vid;
}

void SpawnShopTestOwnedItem(
    State& state,
    std::optional<VID> shop_vid,
    const ShopTestItemSpec& spec
) {
    const std::optional<VID> item_vid = SpawnStageEntityAtTopLeft(
        state,
        spec.type_,
        Vec2::New(
            static_cast<float>(spec.tile_x * static_cast<int>(kTileSize)),
            10.0F * static_cast<float>(kTileSize) - GetEntityArchetype(spec.type_).size.y
        )
    );
    if (!item_vid.has_value()) {
        return;
    }

    Entity* const item = state.entity_manager.GetEntityMut(*item_vid);
    if (item == nullptr) {
        return;
    }

    item->buyable.active = true;
    item->buyable.display_quantity = spec.price;
    item->buyable.display_icon_animation_id = frame_data_ids::GoldIcon;
    item->buyable.shop_owner_vid = shop_vid;
    item->buyable.on_try_buy = spec.type_ == EntityType::Damsel
                                 ? entities::damsel::BuyDamsel
                                 : TryBuyEntityForMoney;
    if (!shop_vid.has_value()) {
        return;
    }

    if (Entity* const shop = state.entity_manager.GetEntityMut(*shop_vid)) {
        entities::shop::AddShopChild(*shop, *item_vid);
    }
}

void SpawnShopTestSign(State& state, EntityType type_, int tile_x) {
    (void)SpawnStageEntityAtTopLeft(
        state,
        type_,
        Vec2::New(
            static_cast<float>(tile_x * static_cast<int>(kTileSize)),
            5.0F * static_cast<float>(kTileSize)
        )
    );
}

void SpawnShopTestStoreLight(State& state, int anchor_tile_x, int start_tile_y) {
    const std::optional<Vec2> pos =
        FindStoreLightTopLeftFromAnchor(state.stage, anchor_tile_x, start_tile_y);
    if (!pos.has_value()) {
        return;
    }
    (void)SpawnStageEntityAtTopLeft(state, EntityType::StoreLight, *pos);
}

void BuildMazeDoorTestPerimeter(Stage& stage) {
    for (int x = 0; x < kMazeDoorTestStageWidthTiles; ++x) {
        SetStageTile(stage, x, 0, Tile::CaveDirt);
        SetStageTile(stage, x, kMazeDoorTestStageHeightTiles - 1, Tile::CaveDirt);
    }
    for (int y = 0; y < kMazeDoorTestStageHeightTiles; ++y) {
        SetStageTile(stage, 0, y, Tile::CaveDirt);
        SetStageTile(stage, kMazeDoorTestStageWidthTiles - 1, y, Tile::CaveDirt);
    }
}

Stage MakeBowlingTestStage() {
    Stage stage;
    stage.stage_type = StageType::Test1;
    stage.tiles = std::vector<std::vector<Tile>>(
        static_cast<std::size_t>(kBowlingTestStageHeightTiles),
        std::vector<Tile>(static_cast<std::size_t>(kBowlingTestStageWidthTiles), Tile::Air)
    );
    FillDebugStageBackwall(stage);
    stage.rooms = {};
    stage.path = {};
    stage.gravity = 0.3F;
    stage.border = Stage::MakeUniformBorder(kDefaultDebugBorderTile);
    stage.camera_clamp_margin = ToVec2(Stage::kRoomShape * kTileSize) / 2.0F;
    stage.camera_clamp_enabled = true;

    for (int x = 0; x < kBowlingTestStageWidthTiles; ++x) {
        stage.tiles[static_cast<std::size_t>(kBowlingTestStageHeightTiles - 1)][static_cast<std::size_t>(x)] =
            DirtTileForFamilyTile(stage.border.left.tile);
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
    FillDebugStageBackwall(stage);
    stage.rooms = {};
    stage.path = {};
    stage.gravity = 0.3F;
    stage.border = Stage::MakeUniformBorder(kDefaultDebugBorderTile);
    stage.camera_clamp_margin = ToVec2(Stage::kRoomShape * kTileSize) / 2.0F;
    stage.camera_clamp_enabled = true;

    for (int x = 0; x < kOpposingBodySmackStageWidthTiles; ++x) {
        stage.tiles[static_cast<std::size_t>(kOpposingBodySmackStageHeightTiles - 1)]
                   [static_cast<std::size_t>(x)] = DirtTileForFamilyTile(stage.border.left.tile);
    }

    return stage;
}

Stage MakeShopTestStage() {
    Stage stage;
    stage.stage_type = StageType::Test1;
    stage.tiles = std::vector<std::vector<Tile>>(
        static_cast<std::size_t>(kShopTestStageHeightTiles),
        std::vector<Tile>(static_cast<std::size_t>(kShopTestStageWidthTiles), Tile::Air)
    );
    FillDebugStageBackwall(stage);
    stage.rooms = {};
    stage.path = {};
    stage.gravity = 0.3F;
    stage.border = Stage::MakeUniformBorder(kDefaultDebugBorderTile);
    stage.camera_clamp_margin = ToVec2(Stage::kRoomShape * kTileSize) / 2.0F;
    stage.camera_clamp_enabled = true;

    const Tile floor_tile = DirtTileForFamilyTile(stage.border.left.tile);
    for (int x = 0; x < kShopTestStageWidthTiles; ++x) {
        stage.tiles[static_cast<std::size_t>(kShopTestStageHeightTiles - 1)]
                   [static_cast<std::size_t>(x)] = floor_tile;
    }

    constexpr std::array<ShopTestStallSpec, 3> kStalls{{
        ShopTestStallSpec{.left_x = 10, .right_x = 24},
        ShopTestStallSpec{.left_x = 30, .right_x = 46},
        ShopTestStallSpec{.left_x = 52, .right_x = 68},
    }};
    for (const ShopTestStallSpec& stall : kStalls) {
        BuildShopTestStall(stage, stall);
    }

    return stage;
}

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

Stage MakeMazeDoorTestStage(MazeDoorTestRoom room) {
    Stage stage;
    stage.stage_type = StageType::Test1;
    stage.tiles = std::vector<std::vector<Tile>>(
        static_cast<std::size_t>(kMazeDoorTestStageHeightTiles),
        std::vector<Tile>(static_cast<std::size_t>(kMazeDoorTestStageWidthTiles), Tile::Air)
    );
    stage.FillBackwall(std::vector<Tile>(kCaveBackwallFillTiles.begin(), kCaveBackwallFillTiles.end()));
    stage.rooms = {};
    stage.path = {};
    stage.gravity = 0.3F;
    stage.border = Stage::MakeUniformBorder(kDefaultDebugBorderTile);
    stage.camera_clamp_margin = ToVec2(Stage::kRoomShape * kTileSize) / 2.0F;
    stage.camera_clamp_enabled = true;

    BuildMazeDoorTestPerimeter(stage);
    switch (room) {
    case MazeDoorTestRoom::RoomA:
        for (int x = 5; x <= 8; ++x) {
            SetStageTile(stage, x, 4, Tile::CaveDirt);
        }
        break;
    case MazeDoorTestRoom::RoomB:
        for (int y = 2; y <= 5; ++y) {
            SetStageTile(stage, 6, y, Tile::CaveDirt);
        }
        SetStageTile(stage, 3, 4, Tile::CaveDirt);
        SetStageTile(stage, 8, 3, Tile::CaveDirt);
        break;
    case MazeDoorTestRoom::RoomC:
        for (int x = 3; x <= 5; ++x) {
            SetStageTile(stage, x, 4, Tile::CaveDirt);
        }
        for (int x = 7; x <= 9; ++x) {
            SetStageTile(stage, x, 3, Tile::CaveDirt);
        }
        break;
    }

    return stage;
}

void InitCommonStageState(State& state) {
    state.stage_frame = 0;
    state.entity_manager.ClearAllEntities();
    state.entity_tools.tool_states.clear();
    state.contact = ContactBookkeeping{};
    state.particles.Clear();
    state.player_vid.reset();
    state.controlled_entity_vid.reset();
    state.mouse_trailer_vid.reset();
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
                    state.entity_tools.EnsureToolSlot(*player_vid, 0),
                    *bomb_tool_kind,
                    static_cast<std::uint16_t>(player->bombs),
                    true
                );
            }

            if (const std::optional<ToolKind> rope_tool_kind = FindPreferredToolKindForSlotIndex(1)) {
                FillToolSlot(
                    state.entity_tools.EnsureToolSlot(*player_vid, 1),
                    *rope_tool_kind,
                    static_cast<std::uint16_t>(player->ropes),
                    true
                );
            }
        }
    }
}


void PlacePlayerAtPosition(State& state, const Vec2& pos) {
    if (!state.player_vid.has_value()) {
        return;
    }
    Entity* const player = state.entity_manager.GetEntityMut(*state.player_vid);
    if (player == nullptr) {
        return;
    }
    player->pos = pos;
    player->vel = Vec2::New(0.0F, 0.0F);
    player->acc = Vec2::New(0.0F, 0.0F);
}

Vec2 GetMazeDoorTestPlayerSpawn(MazeDoorTestRoom room) {
    switch (room) {
    case MazeDoorTestRoom::RoomA:
        return Vec2::New(2.0F * static_cast<float>(kTileSize), 6.0F * static_cast<float>(kTileSize) - 14.0F);
    case MazeDoorTestRoom::RoomB:
        return Vec2::New(2.0F * static_cast<float>(kTileSize), 6.0F * static_cast<float>(kTileSize) - 14.0F);
    case MazeDoorTestRoom::RoomC:
        return Vec2::New(2.0F * static_cast<float>(kTileSize), 6.0F * static_cast<float>(kTileSize) - 14.0F);
    }
    return Vec2::New(2.0F * static_cast<float>(kTileSize), 6.0F * static_cast<float>(kTileSize) - 14.0F);
}

void SpawnMazeDoorLogo(State& state, const Vec2& pos, const Vec2& vel, MazeDoorTestRoom target_room) {
    const std::optional<VID> vid = state.entity_manager.NewEntity();
    if (!vid) {
        return;
    }
    Entity* const entity = state.entity_manager.GetEntityMut(*vid);
    if (entity == nullptr) {
        return;
    }

    SetEntityAs(*entity, EntityType::DvdLogo);
    entity->pos = pos;
    entity->vel = vel;
    entity->transition_target = StageTransitionTarget{
        .destination = StageLoadTarget::ForDebugLevel(
            DebugLevelKind::MazeDoorTest,
            static_cast<std::uint8_t>(target_room)
        ),
        .preserve_player_state = true,
    };
}

StageCarryover CaptureStageCarryover(const State& state) {
    StageCarryover carryover;
    if (!state.player_vid.has_value()) {
        return carryover;
    }

    const Entity* const player = state.entity_manager.GetEntity(*state.player_vid);
    if (player == nullptr || !player->active || player->condition == EntityCondition::Dead) {
        return carryover;
    }

    carryover.player = *player;
    carryover.player->holding = false;
    carryover.player->holding_vid.reset();
    carryover.player->back_vid.reset();

    if (player->holding_vid.has_value()) {
        if (const Entity* const held_item = state.entity_manager.GetEntity(*player->holding_vid)) {
            if (held_item->active) {
                carryover.held_item = *held_item;
                carryover.player->holding_vid = held_item->vid;
                carryover.player->holding = true;
            }
        }
    }

    if (player->back_vid.has_value()) {
        if (const Entity* const back_item = state.entity_manager.GetEntity(*player->back_vid)) {
            if (back_item->active) {
                carryover.back_item = *back_item;
                carryover.player->back_vid = back_item->vid;
            }
        }
    }

    if (const EntityToolState* const tools = state.entity_tools.FindEntityToolState(player->vid)) {
        carryover.player_tools = *tools;
    }

    return carryover;
}

void RestoreEntitySlot(EntityManager& entity_manager, const Entity& entity) {
    if (entity.vid.id >= entity_manager.entities.size()) {
        return;
    }

    entity_manager.entities[entity.vid.id] = entity;
    entity_manager.entities[entity.vid.id].active = true;

    const auto it = std::find(
        entity_manager.available_ids.begin(),
        entity_manager.available_ids.end(),
        entity.vid.id
    );
    if (it != entity_manager.available_ids.end()) {
        entity_manager.available_ids.erase(it);
    }
}

void PrepareEntityForStageEntry(Entity& entity) {
    entity.marked_for_destruction = false;
    entity.vel = Vec2::New(0.0F, 0.0F);
    entity.acc = Vec2::New(0.0F, 0.0F);
    entity.grounded = false;
    entity.coyote_time = 0;
    entity.dist_traveled_this_frame = 0.0F;
    entity.jumped_this_frame = false;
    entity.use_state = UseState{};
    entity.collided = false;
    entity.collided_last_frame = false;
    entity.contact_sound_cooldown = 0;
    entity.thrown_by.reset();
    entity.thrown_immunity_timer = 0;
    entity.projectile_contact_damage_type = DamageType::Attack;
    entity.projectile_contact_damage_amount = 1;
    entity.projectile_contact_timer = 0;
}

void RestoreStageCarryover(State& state, const StageCarryover& carryover) {
    if (!carryover.player.has_value()) {
        return;
    }

    Entity player = *carryover.player;
    PrepareEntityForStageEntry(player);
    RestoreEntitySlot(state.entity_manager, player);
    state.player_vid = player.vid;
    state.controlled_entity_vid = player.vid;

    if (carryover.player_tools.has_value()) {
        state.entity_tools.tool_states.push_back(*carryover.player_tools);
    }

    if (carryover.held_item.has_value()) {
        Entity held_item = *carryover.held_item;
        PrepareEntityForStageEntry(held_item);
        held_item.held_by_vid = player.vid;
        held_item.attachment_mode = AttachmentMode::Held;
        held_item.has_physics = false;
        held_item.can_collide = false;
        RestoreEntitySlot(state.entity_manager, held_item);
    }

    if (carryover.back_item.has_value()) {
        Entity back_item = *carryover.back_item;
        PrepareEntityForStageEntry(back_item);
        back_item.held_by_vid = player.vid;
        back_item.attachment_mode = AttachmentMode::Back;
        back_item.has_physics = false;
        back_item.can_collide = false;
        RestoreEntitySlot(state.entity_manager, back_item);
    }
}

void PlacePlayerAtEntrance(State& state) {
    const IVec2 starting_room = state.stage.GetStartingRoom();
    const auto [starting_room_tl, starting_room_br] =
        state.stage.GetRoomCorners(ToUVec2(starting_room));

    bool door_found = false;
    for (unsigned int y = starting_room_tl.y; y < starting_room_br.y && !door_found; ++y) {
        for (unsigned int x = starting_room_tl.x; x < starting_room_br.x; ++x) {
            if (state.stage.GetTile(x, y) != Tile::Entrance) {
                continue;
            }

            if (state.player_vid.has_value()) {
                if (Entity* const player = state.entity_manager.GetEntityMut(*state.player_vid)) {
                    player->pos = Vec2::New(static_cast<float>(x), static_cast<float>(y)) *
                                  static_cast<float>(kTileSize);
                    player->vel = Vec2::New(0.0F, 0.0F);
                    player->acc = Vec2::New(0.0F, 0.0F);
                }
            }
            door_found = true;
            break;
        }
    }

    if (!door_found) {
        throw std::runtime_error(
            "No door found in starting room. You have a game breaking bug in the map generation "
            "code. Don't ship with this in.");
    }
}

void SnapAttachedItemsToPlayer(State& state) {
    if (!state.player_vid.has_value()) {
        return;
    }

    Entity* const player = state.entity_manager.GetEntityMut(*state.player_vid);
    if (player == nullptr) {
        return;
    }

    const Vec2 player_center = player->GetCenter();

    if (player->holding_vid.has_value()) {
        if (Entity* const held_item = state.entity_manager.GetEntityMut(*player->holding_vid)) {
            const Vec2 hold_offset = Vec2::New(4.0F, 1.0F);
            held_item->facing = player->facing;
            held_item->draw_layer = DrawLayer::Foreground;
            held_item->SetCenter(
                player->facing == LeftOrRight::Left
                    ? player_center + Vec2::New(-hold_offset.x, hold_offset.y)
                    : player_center + hold_offset
            );
        }
    }

    if (player->back_vid.has_value()) {
        if (Entity* const back_item = state.entity_manager.GetEntityMut(*player->back_vid)) {
            const Vec2 back_offset = Vec2::New(-3.0F, 0.0F);
            back_item->facing = player->facing;
            back_item->draw_layer = DrawLayer::Background;
            TrySetAnimation(*back_item, EntityDisplayState::Neutral);
            back_item->SetCenter(
                player->facing == LeftOrRight::Left
                    ? player_center + Vec2::New(-back_offset.x, back_offset.y)
                    : player_center + back_offset
            );
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
        if (spawn.type_ == EntityType::StoreLight) {
            entities::store_light::AttachStoreLight(*entity, state);
        }
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

std::optional<VID> SpawnStageEntityAtTopLeft(State& state, EntityType type_, const Vec2& pos) {
    const std::optional<VID> vid = state.entity_manager.NewEntity();
    if (!vid.has_value()) {
        return std::nullopt;
    }

    Entity* const entity = state.entity_manager.GetEntityMut(*vid);
    if (entity == nullptr) {
        return std::nullopt;
    }

    SetEntityAs(*entity, type_);
    entity->pos = pos;
    entity->vel = Vec2::New(0.0F, 0.0F);
    if (type_ == EntityType::StoreLight) {
        entities::store_light::AttachStoreLight(*entity, state);
    }
    return vid;
}

void InitShopTestStage(State& state) {
    InitCommonStageState(state);
    state.mouse_trailer_vid.reset();

    const float player_spawn_x = static_cast<float>(3 * static_cast<int>(kTileSize));
    const float player_spawn_y = static_cast<float>(10 * static_cast<int>(kTileSize) - 14);
    SpawnPlayer(state, Vec2::New(player_spawn_x, player_spawn_y));
    if (state.player_vid.has_value()) {
        if (Entity* const player = state.entity_manager.GetEntityMut(*state.player_vid)) {
            player->money = 50000;
        }
    }

    (void)SpawnStageEntityAtTopLeft(
        state,
        EntityType::GoldIdol,
        Vec2::New(4.0F * static_cast<float>(kTileSize), 10.0F * static_cast<float>(kTileSize) - 16.0F)
    );

    const std::optional<VID> left_shop_vid =
        SpawnShopTestShop(state, ShopTestStallSpec{.left_x = 10, .right_x = 24}, 12);
    SpawnShopTestOwnedItem(
        state,
        left_shop_vid,
        ShopTestItemSpec{.type_ = EntityType::Mattock, .tile_x = 16, .price = 2000}
    );
    SpawnShopTestOwnedItem(
        state,
        left_shop_vid,
        ShopTestItemSpec{.type_ = EntityType::BombBag, .tile_x = 20, .price = 2500}
    );
    SpawnShopTestSign(state, EntityType::SignGeneral, 15);
    SpawnShopTestSign(state, EntityType::SignBomb, 19);
    SpawnShopTestStoreLight(state, 14, 9);
    SpawnShopTestStoreLight(state, 20, 9);

    const std::optional<VID> middle_shop_vid =
        SpawnShopTestShop(state, ShopTestStallSpec{.left_x = 30, .right_x = 46}, 32);
    SpawnShopTestOwnedItem(
        state,
        middle_shop_vid,
        ShopTestItemSpec{.type_ = EntityType::Shotgun, .tile_x = 35, .price = 12000}
    );
    SpawnShopTestOwnedItem(
        state,
        middle_shop_vid,
        ShopTestItemSpec{.type_ = EntityType::JetPack, .tile_x = 39, .price = 20000}
    );
    SpawnShopTestOwnedItem(
        state,
        middle_shop_vid,
        ShopTestItemSpec{.type_ = EntityType::Cape, .tile_x = 43, .price = 8000}
    );
    SpawnShopTestSign(state, EntityType::SignWeapon, 34);
    SpawnShopTestSign(state, EntityType::SignRare, 38);
    SpawnShopTestSign(state, EntityType::SignClothing, 42);
    SpawnShopTestStoreLight(state, 34, 9);
    SpawnShopTestStoreLight(state, 42, 9);

    const std::optional<VID> right_shop_vid =
        SpawnShopTestShop(state, ShopTestStallSpec{.left_x = 52, .right_x = 68}, 54);
    SpawnShopTestOwnedItem(
        state,
        right_shop_vid,
        ShopTestItemSpec{.type_ = EntityType::Dice, .tile_x = 57, .price = 5000}
    );
    SpawnShopTestOwnedItem(
        state,
        right_shop_vid,
        ShopTestItemSpec{.type_ = EntityType::Damsel, .tile_x = 62, .price = 12000}
    );
    SpawnShopTestSign(state, EntityType::SignCraps, 56);
    SpawnShopTestSign(state, EntityType::SignKissing, 61);
    SpawnShopTestStoreLight(state, 56, 9);
    SpawnShopTestStoreLight(state, 64, 9);
}

void InitBowlingTestStage(State& state) {
    InitCommonStageState(state);
    state.mouse_trailer_vid.reset();

    const float player_spawn_x = static_cast<float>(4 * static_cast<int>(kTileSize));
    const float player_spawn_y = static_cast<float>(3 * static_cast<int>(kTileSize) - 10);
    SpawnPlayer(state, Vec2::New(player_spawn_x, player_spawn_y));

    const float caveman_center_y =
        static_cast<float>((kBowlingTestStageHeightTiles - 1) * static_cast<int>(kTileSize)) - 8.0F;
    const std::optional<VID> launched_caveman_vid = SpawnBowlingCaveman(
        state,
        Vec2::New(8.0F * static_cast<float>(kTileSize), caveman_center_y),
        EntityCondition::Stunned,
        Vec2::New(24.0F, 0.0F)
    );
    (void)launched_caveman_vid;
    for (Entity& entity : state.entity_manager.entities) {
        if (!entity.active || entity.type_ != EntityType::Caveman ||
            entity.condition != EntityCondition::Stunned) {
            continue;
        }
        entity.max_speed = 24.0F;
        entity.has_ground_friction = false;
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

void InitMazeDoorTestStage(State& state, bool preserve_player_state) {
    const StageCarryover carryover =
        preserve_player_state ? CaptureStageCarryover(state) : StageCarryover{};
    InitCommonStageState(state);
    state.mouse_trailer_vid.reset();

    const MazeDoorTestRoom room = state.debug_level.maze_door_test.room;
    const Vec2 spawn_pos = GetMazeDoorTestPlayerSpawn(room);
    if (carryover.player.has_value()) {
        RestoreStageCarryover(state, carryover);
        PlacePlayerAtPosition(state, spawn_pos);
        SnapAttachedItemsToPlayer(state);
    } else {
        SpawnPlayer(state, spawn_pos);
    }

    switch (room) {
    case MazeDoorTestRoom::RoomA:
        SpawnMazeDoorLogo(
            state,
            Vec2::New(8.0F * static_cast<float>(kTileSize), 2.0F * static_cast<float>(kTileSize)),
            Vec2::New(-1.0F, 1.0F),
            MazeDoorTestRoom::RoomB
        );
        break;
    case MazeDoorTestRoom::RoomB:
        SpawnMazeDoorLogo(
            state,
            Vec2::New(2.0F * static_cast<float>(kTileSize), 2.0F * static_cast<float>(kTileSize)),
            Vec2::New(1.0F, 1.0F),
            MazeDoorTestRoom::RoomA
        );
        SpawnMazeDoorLogo(
            state,
            Vec2::New(8.0F * static_cast<float>(kTileSize), 2.0F * static_cast<float>(kTileSize)),
            Vec2::New(-1.0F, 1.0F),
            MazeDoorTestRoom::RoomC
        );
        break;
    case MazeDoorTestRoom::RoomC:
        SpawnMazeDoorLogo(
            state,
            Vec2::New(6.0F * static_cast<float>(kTileSize), 2.0F * static_cast<float>(kTileSize)),
            Vec2::New(1.0F, 1.0F),
            MazeDoorTestRoom::RoomA
        );
        break;
    }
}

} // namespace

void InitStage(State& state, bool preserve_player_state) {
    state.respawn_target = StageLoadTarget::ForStageType(StageType::SplkMines1);
    const StageCarryover carryover =
        preserve_player_state ? CaptureStageCarryover(state) : StageCarryover{};
    InitCommonStageState(state);

    if (carryover.player.has_value()) {
        RestoreStageCarryover(state, carryover);
    } else {
        SpawnPlayer(state, Vec2::New(0.0F, 0.0F));
    }
    SpawnAuthoredStageEntities(state);

    if (!stage_gen::splk_mines::UsesSplkMinesGenerator(state.stage.stage_type)) {
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

    PlacePlayerAtEntrance(state);
    if (carryover.player.has_value()) {
        SnapAttachedItemsToPlayer(state);
    }
}

void InitDebugLevel(State& state, bool preserve_player_state) {
    state.respawn_target = StageLoadTarget::ForDebugLevel(state.debug_level.kind);
    switch (state.debug_level.kind) {
    case DebugLevelKind::SplkMines1:
        state.stage = Stage::New(StageType::SplkMines1);
        InitStage(state, preserve_player_state);
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
    case DebugLevelKind::MazeDoorTest:
        state.stage = MakeMazeDoorTestStage(state.debug_level.maze_door_test.room);
        state.respawn_target = StageLoadTarget::ForDebugLevel(
            DebugLevelKind::MazeDoorTest,
            static_cast<std::uint8_t>(MazeDoorTestRoom::RoomA)
        );
        InitMazeDoorTestStage(state, preserve_player_state);
        break;
    case DebugLevelKind::BowlingTest:
        state.stage = MakeBowlingTestStage();
        InitBowlingTestStage(state);
        break;
    case DebugLevelKind::OpposingBodySmack:
        state.stage = MakeOpposingBodySmackStage();
        InitOpposingBodySmackStage(state);
        break;
    case DebugLevelKind::ShopTest:
        state.stage = MakeShopTestStage();
        InitShopTestStage(state);
        break;
    }
}

} // namespace splonks

#include "debug/shop_test_stage.hpp"

#include "buying.hpp"
#include "ents/shop.hpp"
#include "ents/shop_tile_triggers.hpp"
#include "aframe_id.hpp"
#include "player_queries.hpp"
#include "stage_spawning.hpp"
#include "tile_spec.hpp"

#include <array>
#include <optional>
#include <vector>

namespace splonks {

namespace {

constexpr int kShopTestStageWidthTiles = 80;
constexpr int kShopTestStageHeightTiles = 12;
constexpr Tile kDefaultDebugBorderTile = Tile::CaveDirt;
constexpr std::array<Tile, 3> kCaveBackwallFillTiles{{
    Tile::CaveAir0,
    Tile::CaveAir1,
    Tile::CaveAir2,
}};

struct ShopTestStallSpec {
    int left_x = 0;
    int right_x = 0;
};

struct ShopTestItemSpec {
    EntType type_ = EntType::None;
    int tile_x = 0;
    std::uint32_t price = 0;
};

void FillDebugStageBackwall(Stage& stage) {
    stage.FillBackwall(std::vector<Tile>(kCaveBackwallFillTiles.begin(), kCaveBackwallFillTiles.end()));
}

void SetStageTile(Stage& stage, int x, int y, Tile tile) {
    stage.SetTile(IVec2::New(x, y), tile);
}

void SetStageBackwallTile(Stage& stage, int x, int y, Tile tile) {
    if (x < 0 || y < 0 || x >= static_cast<int>(stage.GetTileWidth()) ||
        y >= static_cast<int>(stage.GetTileHeight())) {
        return;
    }
    stage.SetBackwallTile(IVec2::New(x, y), tile);
}

std::optional<Vec2> FindStoreLightTopLeftFromAnchor(
    const Stage& stage,
    int anchor_tile_x,
    int start_tile_y
) {
    if (anchor_tile_x < 0 || anchor_tile_x >= static_cast<int>(stage.GetTileWidth())) {
        return std::nullopt;
    }

    for (int y = start_tile_y; y >= 0; --y) {
        if (!GetTileSpec(stage.GetTile(
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

void AddShopTestVandalismTrigger(State& state, const IVec2& tile_pos, VID shop_vid) {
    const StageTileTrigger trigger = ents::shop::MakeShopVandalismTileTrigger(tile_pos, shop_vid);
    state.stage.tile_triggers.push_back(trigger);
    state.stage.stagegen_annotations.push_back(StageGenAnnotation{
        .world_pos = ToVec2(tile_pos * static_cast<int>(kTileSize)) + Vec2::New(2.0F, 8.0F),
        .text = trigger.debug_label == nullptr ? "" : trigger.debug_label,
    });
}

void AddShopTestVandalismTriggers(State& state, const ShopTestStallSpec& stall, std::optional<VID> shop_vid) {
    if (!shop_vid.has_value()) {
        return;
    }

    constexpr int kShopTopY = 4;
    constexpr int kShopBottomY = 10;
    for (int x = stall.left_x; x <= stall.right_x; ++x) {
        AddShopTestVandalismTrigger(state, IVec2::New(x, kShopTopY), *shop_vid);
    }

    for (int y = kShopTopY + 1; y <= kShopBottomY - 2; ++y) {
        AddShopTestVandalismTrigger(state, IVec2::New(stall.left_x, y), *shop_vid);
        AddShopTestVandalismTrigger(state, IVec2::New(stall.right_x, y), *shop_vid);
    }

    const int floor_y = static_cast<int>(state.stage.GetTileHeight()) - 1;
    for (int x = stall.left_x; x <= stall.right_x; ++x) {
        AddShopTestVandalismTrigger(state, IVec2::New(x, floor_y), *shop_vid);
    }
}

sim::AABB MakeShopTestArea(const ShopTestStallSpec& stall) {
    constexpr int kShopTopY = 4;
    constexpr int kShopBottomY = 10;
    return sim::AABB::from_corners(
        sim::PixelVec2(stall.left_x * static_cast<int>(kTileSize),
                       kShopTopY * static_cast<int>(kTileSize)),
        sim::PixelVec2((stall.right_x + 1) * static_cast<int>(kTileSize) - 1,
                       (kShopBottomY + 1) * static_cast<int>(kTileSize) - 1)
    );
}

std::optional<VID> SpawnShopTestShop(
    State& state,
    const ShopTestStallSpec& stall,
    int shopkeeper_tile_x
) {
    const sim::AABB shop_area = MakeShopTestArea(stall);
    const std::optional<VID> shop_vid =
        SpawnStageEntAtTopLeft(state, EntType::Shop, shop_area.tl);
    if (!shop_vid.has_value()) {
        return std::nullopt;
    }

    Ent* const shop = state.ents.GetEntMut(*shop_vid);
    if (shop == nullptr) {
        return std::nullopt;
    }
    ents::shop::SetShopArea(*shop, shop_area);

    const std::optional<VID> shopkeeper_vid = SpawnStageEntAtTopLeft(
        state,
        EntType::Shopkeeper,
        Vec2::New(
            static_cast<float>(shopkeeper_tile_x * static_cast<int>(kTileSize)),
            9.0F * static_cast<float>(kTileSize) - 16.0F
        )
    );
    if (shopkeeper_vid.has_value()) {
        shop->ent_a = *shopkeeper_vid;
        if (Ent* const shopkeeper = state.ents.GetEntMut(*shopkeeper_vid)) {
            shopkeeper->ent_a = *shop_vid;
        }
    }

    return shop_vid;
}

void SpawnShopTestOwnedItem(
    State& state,
    std::optional<VID> shop_vid,
    const ShopTestItemSpec& spec
) {
    const std::optional<VID> item_vid = SpawnStageEntAtTopLeft(
        state,
        spec.type_,
        Vec2::New(
            static_cast<float>(spec.tile_x * static_cast<int>(kTileSize)),
            10.0F * static_cast<float>(kTileSize) -
                sim::ToRenderScalar(GetEntSpec(spec.type_).size.y)
        )
    );
    if (!item_vid.has_value()) {
        return;
    }

    Ent* const item = state.ents.GetEntMut(*item_vid);
    if (item == nullptr) {
        return;
    }

    ConfigureEntAsBuyable(*item, spec.price);
    item->buyable.shop_owner_vid = shop_vid;
    if (!shop_vid.has_value()) {
        return;
    }

    if (Ent* const shop = state.ents.GetEntMut(*shop_vid)) {
        ents::shop::AddShopChild(*shop, *item_vid);
    }
}

void SpawnShopTestCrapsTable(
    State& state,
    std::optional<VID> shop_vid,
    const ShopTestStallSpec& stall
) {
    const std::optional<VID> dice_vid = SpawnStageEntAtTopLeft(
        state,
        EntType::Dice,
        Vec2::New(57.0F * static_cast<float>(kTileSize),
                  10.0F * static_cast<float>(kTileSize) -
                      sim::ToRenderScalar(GetEntSpec(EntType::Dice).size.y))
    );
    const std::optional<VID> prize_vid = SpawnStageEntAtTopLeft(
        state,
        EntType::JetPack,
        Vec2::New(62.0F * static_cast<float>(kTileSize),
                  10.0F * static_cast<float>(kTileSize) -
                      sim::ToRenderScalar(GetEntSpec(EntType::JetPack).size.y))
    );
    const sim::AABB shop_area = MakeShopTestArea(stall);
    const std::optional<VID> table_vid =
        SpawnStageEntAtTopLeft(state, EntType::CrapsTable, shop_area.tl);
    if (!table_vid.has_value()) {
        return;
    }

    Ent* const table = state.ents.GetEntMut(*table_vid);
    if (table == nullptr) {
        return;
    }
    table->size = shop_area.br - shop_area.tl + sim::PixelVec2(1, 1);
    table->ent_a = shop_vid;
    table->ent_b = dice_vid;
    table->ent_c = prize_vid;
}

void SpawnShopTestSign(State& state, EntType type_, int tile_x) {
    (void)SpawnStageEntAtTopLeft(
        state,
        type_,
        Vec2::New(
            static_cast<float>(tile_x * static_cast<int>(kTileSize)),
            5.0F * static_cast<float>(kTileSize)
        )
    );
}

} // namespace

void SpawnShopTestStoreLight(State& state, int anchor_tile_x, int start_tile_y) {
    const std::optional<Vec2> pos =
        FindStoreLightTopLeftFromAnchor(state.stage, anchor_tile_x, start_tile_y);
    if (!pos.has_value()) {
        return;
    }
    (void)SpawnStageEntAtTopLeft(state, EntType::StoreLight, *pos);
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
    stage.border = Stage::MakeUniformBorder(kDefaultDebugBorderTile);
    stage.camera_clamp_margin = ToVec2(Stage::kRoomShape * kTileSize) / 2.0F;
    stage.camera_clamp_enabled = true;

    const Tile floor_tile = kDefaultDebugBorderTile;
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

void InitShopTestStage(State& state) {
    InitCommonStageState(state);
    state.mouse_trailer_vid.reset();

    const float player_spawn_x = static_cast<float>(3 * static_cast<int>(kTileSize));
    const float player_spawn_y = static_cast<float>(10 * static_cast<int>(kTileSize) - 14);
    SpawnPlayer(state, Vec2::New(player_spawn_x, player_spawn_y));
    if (Ent* const player = GetPrimaryLocalPlayerMut(state)) {
        player->money = 50000;
    }

    (void)SpawnStageEntAtTopLeft(
        state,
        EntType::GoldIdol,
        Vec2::New(4.0F * static_cast<float>(kTileSize), 10.0F * static_cast<float>(kTileSize) - 16.0F)
    );

    const std::optional<VID> left_shop_vid =
        SpawnShopTestShop(state, ShopTestStallSpec{.left_x = 10, .right_x = 24}, 12);
    AddShopTestVandalismTriggers(state, ShopTestStallSpec{.left_x = 10, .right_x = 24}, left_shop_vid);
    SpawnShopTestOwnedItem(
        state,
        left_shop_vid,
        ShopTestItemSpec{.type_ = EntType::Mattock, .tile_x = 16, .price = 8000}
    );
    SpawnShopTestOwnedItem(
        state,
        left_shop_vid,
        ShopTestItemSpec{.type_ = EntType::BombBag, .tile_x = 20, .price = 2500}
    );
    SpawnShopTestSign(state, EntType::SignGeneral, 15);
    SpawnShopTestSign(state, EntType::SignBomb, 19);
    SpawnShopTestStoreLight(state, 14, 9);
    SpawnShopTestStoreLight(state, 20, 9);

    const std::optional<VID> middle_shop_vid =
        SpawnShopTestShop(state, ShopTestStallSpec{.left_x = 30, .right_x = 46}, 32);
    AddShopTestVandalismTriggers(state, ShopTestStallSpec{.left_x = 30, .right_x = 46}, middle_shop_vid);
    SpawnShopTestOwnedItem(
        state,
        middle_shop_vid,
        ShopTestItemSpec{.type_ = EntType::Shotgun, .tile_x = 35, .price = 15000}
    );
    SpawnShopTestOwnedItem(
        state,
        middle_shop_vid,
        ShopTestItemSpec{.type_ = EntType::JetPack, .tile_x = 39, .price = 20000}
    );
    SpawnShopTestOwnedItem(
        state,
        middle_shop_vid,
        ShopTestItemSpec{.type_ = EntType::Cape, .tile_x = 43, .price = 12000}
    );
    SpawnShopTestSign(state, EntType::SignWeapon, 34);
    SpawnShopTestSign(state, EntType::SignRare, 38);
    SpawnShopTestSign(state, EntType::SignClothing, 42);
    SpawnShopTestStoreLight(state, 34, 9);
    SpawnShopTestStoreLight(state, 42, 9);

    const std::optional<VID> right_shop_vid =
        SpawnShopTestShop(state, ShopTestStallSpec{.left_x = 52, .right_x = 68}, 54);
    AddShopTestVandalismTriggers(state, ShopTestStallSpec{.left_x = 52, .right_x = 68}, right_shop_vid);
    SpawnShopTestCrapsTable(state, right_shop_vid, ShopTestStallSpec{.left_x = 52, .right_x = 68});
    state.stage.background_stamps.push_back(BackgroundStamp{
        .anim_id = aframe_ids::DiceSign,
        .pos = Vec2::New(
            58.0F * static_cast<float>(kTileSize),
            5.0F * static_cast<float>(kTileSize)
        ),
    });
    SpawnShopTestSign(state, EntType::SignCraps, 56);
    SpawnShopTestStoreLight(state, 56, 9);
    SpawnShopTestStoreLight(state, 64, 9);
}

} // namespace splonks

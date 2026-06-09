#include "tile_spec.hpp"

#include "ent/spec.hpp"
#include "state.hpp"
#include "world_ops.hpp"

#include <array>
#include <stdexcept>

namespace splonks {

namespace {

constexpr std::size_t TileIndex(Tile tile) {
    return static_cast<std::size_t>(tile);
}

Ent* SpawnEntAtCenter(EntType type_, sim::FxVec2 center, State& state) {
    return world_ops::SpawnEnt(state, type_, [center](Ent& ent) {
        ent.SetCenter(center);
        ent.vel = sim::FxVec2::zero();
    });
}

void OnBreakAsBigGoldMaterial(const IVec2& tile_pos, State& state, Audio& audio) {
    (void)audio;
    const sim::FxVec2 center = sim::PixelVec2(
        tile_pos.x * static_cast<int>(kTileSize) + 8,
        tile_pos.y * static_cast<int>(kTileSize) + 8
    );
    SpawnEntAtCenter(EntType::GoldChunk, center + sim::PixelVec2(-4, -1), state);
    SpawnEntAtCenter(EntType::GoldChunk, center + sim::PixelVec2(0, 1), state);
    SpawnEntAtCenter(EntType::GoldChunk, center + sim::PixelVec2(4, -1), state);
    SpawnEntAtCenter(EntType::GoldNugget, center, state);
}

TileSpec MakeSolidTileSpec(
    Tile tile, AFrameId break_anim, const char* debug_name,
    std::optional<AudioAssetId> break_sound = audio_asset_ids::Thud, TileOnBreak on_break = nullptr,
    float friction = 0.85F, bool hangable = true) {
    return TileSpec{
        .tile = tile,
        .solid = true,
        .one_way_top_solid = false,
        .climbable = false,
        .transparent = false,
        .hangable = hangable,
        .friction = friction,
        .collide_sound = audio_asset_ids::Thud,
        .break_sound = break_sound,
        .break_anim = break_anim,
        .on_break = on_break,
        .debug_name = debug_name,
    };
}

static_assert(TileIndex(Tile::Exit) + 1 <= kTileCount);

const std::array<TileSpec, kTileCount> kTileSpecs{{
    TileSpec{
        .tile = Tile::Air,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .debug_name = "Air",
    },
    TileSpec{
        .tile = Tile::CaveAir0,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .debug_name = "CaveAir0",
    },
    TileSpec{
        .tile = Tile::CaveAir1,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .debug_name = "CaveAir1",
    },
    TileSpec{
        .tile = Tile::CaveAir2,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .debug_name = "CaveAir2",
    },
    MakeSolidTileSpec(Tile::CaveDirt, HashAFrameIdConstexpr("cave_dirt_0"), "CaveDirt"),
    MakeSolidTileSpec(Tile::CaveBlock, HashAFrameIdConstexpr("cave_block_0"), "CaveBlock"),
    MakeSolidTileSpec(Tile::CaveShopWall, HashAFrameIdConstexpr("cave_shop_wall"),
                           "CaveShopWall"),
    MakeSolidTileSpec(Tile::CaveSmoothWall, HashAFrameIdConstexpr("cave_smooth_wall"),
                           "CaveSmoothWall"),
    MakeSolidTileSpec(Tile::Glass, HashAFrameIdConstexpr("glass"), "Glass"),
    MakeSolidTileSpec(Tile::LawsonWall, HashAFrameIdConstexpr("lawson_wall"),
                           "LawsonWall"),
    TileSpec{
        .tile = Tile::LawsonInside,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .debug_name = "LawsonInside",
    },
    MakeSolidTileSpec(Tile::LawsonLeftTopper, HashAFrameIdConstexpr("lawson_left_topper"),
                           "LawsonLeftTopper"),
    MakeSolidTileSpec(Tile::LawsonMiddleTopper, HashAFrameIdConstexpr("lawson_middle_topper"),
                           "LawsonMiddleTopper"),
    MakeSolidTileSpec(Tile::LawsonRightTopper, HashAFrameIdConstexpr("lawson_right_topper"),
                           "LawsonRightTopper"),
    MakeSolidTileSpec(Tile::LawsonFloor, HashAFrameIdConstexpr("lawson_floor"),
                           "LawsonFloor"),
    TileSpec{
        .tile = Tile::IceAir0,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .debug_name = "IceAir0",
    },
    TileSpec{
        .tile = Tile::IceAir1,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .debug_name = "IceAir1",
    },
    TileSpec{
        .tile = Tile::IceAir2,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .debug_name = "IceAir2",
    },
    MakeSolidTileSpec(Tile::IceDirt, HashAFrameIdConstexpr("ice_dirt_0"), "IceDirt",
                           audio_asset_ids::Thud, nullptr, 1.0F, false),
    MakeSolidTileSpec(Tile::IceBlock, HashAFrameIdConstexpr("ice_block_0"), "IceBlock",
                           audio_asset_ids::Thud, nullptr, 1.0F, false),
    TileSpec{
        .tile = Tile::JungleAir0,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .debug_name = "JungleAir0",
    },
    TileSpec{
        .tile = Tile::JungleAir1,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .debug_name = "JungleAir1",
    },
    TileSpec{
        .tile = Tile::JungleAir2,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .debug_name = "JungleAir2",
    },
    MakeSolidTileSpec(Tile::JungleDirt, HashAFrameIdConstexpr("jungle_dirt_0"),
                           "JungleDirt"),
    MakeSolidTileSpec(Tile::JungleBlock, HashAFrameIdConstexpr("jungle_block_0"),
                           "JungleBlock"),
    TileSpec{
        .tile = Tile::TempleAir0,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .debug_name = "TempleAir0",
    },
    TileSpec{
        .tile = Tile::TempleAir1,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .debug_name = "TempleAir1",
    },
    TileSpec{
        .tile = Tile::TempleAir2,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .debug_name = "TempleAir2",
    },
    MakeSolidTileSpec(Tile::TempleDirt, HashAFrameIdConstexpr("temple_dirt_0"),
                           "TempleDirt"),
    MakeSolidTileSpec(Tile::TempleGold, HashAFrameIdConstexpr("temple_gold"), "TempleGold",
                           audio_asset_ids::MoneySmashed, OnBreakAsBigGoldMaterial),
    MakeSolidTileSpec(Tile::TempleBlock, HashAFrameIdConstexpr("temple_block_0"),
                           "TempleBlock"),
    TileSpec{
        .tile = Tile::BossAir0,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .debug_name = "BossAir0",
    },
    TileSpec{
        .tile = Tile::BossAir1,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .debug_name = "BossAir1",
    },
    TileSpec{
        .tile = Tile::BossAir2,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .debug_name = "BossAir2",
    },
    MakeSolidTileSpec(Tile::BossDirt, HashAFrameIdConstexpr("boss_dirt_0"), "BossDirt"),
    MakeSolidTileSpec(Tile::BossBlock, HashAFrameIdConstexpr("boss_block_0"), "BossBlock"),
    TileSpec{
        .tile = Tile::LadderTop,
        .solid = false,
        .one_way_top_solid = true,
        .climbable = true,
        .climbable_rotation_mask = kTileRotationBit0 | kTileRotationBit180,
        .transparent = true,
        .hangable = false,
        .debug_name = "LadderTop",
    },
    TileSpec{
        .tile = Tile::Ladder,
        .solid = false,
        .climbable = true,
        .climbable_rotation_mask = kTileRotationBit0 | kTileRotationBit180,
        .transparent = true,
        .hangable = false,
        .debug_name = "Ladder",
    },
    TileSpec{
        .tile = Tile::LadderOrange,
        .solid = false,
        .climbable = true,
        .climbable_rotation_mask = kTileRotationBit0 | kTileRotationBit180,
        .transparent = true,
        .hangable = false,
        .debug_name = "LadderOrange",
    },
    TileSpec{
        .tile = Tile::Spikes,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .debug_name = "Spikes",
    },
    TileSpec{
        .tile = Tile::Rope,
        .solid = false,
        .climbable = true,
        .climbable_rotation_mask = kTileRotationBit0 | kTileRotationBit180,
        .transparent = true,
        .hangable = false,
        .debug_name = "Rope",
    },
    TileSpec{
        .tile = Tile::Vine,
        .solid = false,
        .climbable = true,
        .climbable_rotation_mask = kTileRotationBit0 | kTileRotationBit180,
        .transparent = true,
        .hangable = false,
        .debug_name = "Vine",
    },
    TileSpec{
        .tile = Tile::VineTop,
        .solid = false,
        .climbable = true,
        .climbable_rotation_mask = kTileRotationBit0 | kTileRotationBit180,
        .transparent = true,
        .hangable = false,
        .debug_name = "VineTop",
    },
    TileSpec{
        .tile = Tile::WaterSwim,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .simulated_fluid = true,
        .effect_while_inside = EffectId::InWater,
        .debug_name = "WaterSwim",
    },
    TileSpec{
        .tile = Tile::WaterTop,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .simulated_fluid = true,
        .effect_while_inside = EffectId::InWater,
        .debug_name = "WaterTop",
    },
    TileSpec{
        .tile = Tile::Lava,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .debug_name = "Lava",
    },
    MakeSolidTileSpec(Tile::Lush, HashAFrameIdConstexpr("jungle_dirt_0"), "Lush"),
    TileSpec{
        .tile = Tile::Tree,
        .solid = false,
        .climbable = true,
        .transparent = true,
        .hangable = false,
        .debug_name = "Tree",
    },
    MakeSolidTileSpec(Tile::ThinIce, HashAFrameIdConstexpr("ice_block_0"), "ThinIce",
                           audio_asset_ids::Thud, nullptr, 1.0F, false),
    MakeSolidTileSpec(Tile::Dark, HashAFrameIdConstexpr("ice_dirt_0"), "Dark",
                           audio_asset_ids::Thud, nullptr, 1.0F, false),
    MakeSolidTileSpec(Tile::DarkFall, HashAFrameIdConstexpr("ice_block_0"), "DarkFall",
                           audio_asset_ids::Thud, nullptr, 1.0F, false),
    MakeSolidTileSpec(Tile::AlienShip, HashAFrameIdConstexpr("ice_block_0"), "AlienShip",
                           audio_asset_ids::Thud, nullptr, 1.0F, false),
    MakeSolidTileSpec(Tile::TempleFake, HashAFrameIdConstexpr("temple_dirt_0"),
                           "TempleFake"),
    TileSpec{
        .tile = Tile::Entrance,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .render_enabled = false,
        .debug_name = "Entrance",
    },
    TileSpec{
        .tile = Tile::Exit,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .debug_name = "Exit",
    },
}};

} // namespace

const TileSpec& GetTileSpec(Tile tile) {
    const std::size_t index = TileIndex(tile);
    if (index >= kTileCount) {
        throw std::runtime_error("GetTileSpec received invalid tile");
    }
    return kTileSpecs[index];
}

bool IsTileTransparent(Tile tile) {
    return GetTileSpec(tile).transparent;
}

bool IsTileCollidable(Tile tile) {
    return GetTileSpec(tile).solid;
}

bool IsTileOneWayTopSolid(Tile tile) {
    return GetTileSpec(tile).one_way_top_solid;
}

bool IsTileGroundSupport(Tile tile) {
    const TileSpec& spec = GetTileSpec(tile);
    return spec.solid || spec.one_way_top_solid;
}

bool IsTileHangable(Tile tile) {
    return GetTileSpec(tile).hangable;
}

float GetTileFriction(Tile tile) {
    return GetTileSpec(tile).friction;
}

} // namespace splonks

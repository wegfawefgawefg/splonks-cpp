#include "tile_archetype.hpp"

#include "entity/archetype.hpp"
#include "state.hpp"

#include <array>
#include <stdexcept>

namespace splonks {

namespace {

constexpr std::size_t TileIndex(Tile tile) {
    return static_cast<std::size_t>(tile);
}

void SpawnEntityAtCenter(EntityType type_, const Vec2& center, State& state) {
    const std::optional<VID> vid = state.entity_manager.NewEntity();
    if (!vid.has_value()) {
        return;
    }

    Entity* const entity = state.entity_manager.GetEntityMut(*vid);
    if (entity == nullptr) {
        return;
    }

    SetEntityAs(*entity, type_);
    entity->SetCenter(center);
    entity->vel = Vec2::New(0.0F, 0.0F);
}

void OnBreakAsBigGoldMaterial(const IVec2& tile_pos, State& state, Audio& audio) {
    (void)audio;
    const Vec2 center = Vec2::New(
        static_cast<float>(tile_pos.x * static_cast<int>(kTileSize) + 8),
        static_cast<float>(tile_pos.y * static_cast<int>(kTileSize) + 8)
    );
    SpawnEntityAtCenter(EntityType::GoldChunk, center + Vec2::New(-4.0F, -1.0F), state);
    SpawnEntityAtCenter(EntityType::GoldChunk, center + Vec2::New(0.0F, 1.0F), state);
    SpawnEntityAtCenter(EntityType::GoldChunk, center + Vec2::New(4.0F, -1.0F), state);
    SpawnEntityAtCenter(EntityType::GoldNugget, center, state);
}

TileArchetype MakeSolidTileArchetype(
    Tile tile, TileFamily family, FrameDataId break_animation, const char* debug_name,
    std::optional<AudioAssetId> break_sound = audio_asset_ids::Thud, TileOnBreak on_break = nullptr,
    float friction = 0.85F, bool hangable = true) {
    return TileArchetype{
        .tile = tile,
        .solid = true,
        .climbable = false,
        .transparent = false,
        .hangable = hangable,
        .family = family,
        .friction = friction,
        .collide_sound = audio_asset_ids::Thud,
        .break_sound = break_sound,
        .break_animation = break_animation,
        .on_break = on_break,
        .debug_name = debug_name,
    };
}

static_assert(TileIndex(Tile::Exit) + 1 <= kTileCount);

const std::array<TileArchetype, kTileCount> kTileArchetypes{{
    TileArchetype{
        .tile = Tile::Air,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .debug_name = "Air",
    },
    TileArchetype{
        .tile = Tile::CaveAir0,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .family = TileFamily::Cave,
        .debug_name = "CaveAir0",
    },
    TileArchetype{
        .tile = Tile::CaveAir1,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .family = TileFamily::Cave,
        .debug_name = "CaveAir1",
    },
    TileArchetype{
        .tile = Tile::CaveAir2,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .family = TileFamily::Cave,
        .debug_name = "CaveAir2",
    },
    MakeSolidTileArchetype(Tile::CaveDirt, TileFamily::Cave,
                           HashFrameDataIdConstexpr("cave_dirt_0"), "CaveDirt"),
    MakeSolidTileArchetype(Tile::CaveBlock, TileFamily::Cave,
                           HashFrameDataIdConstexpr("cave_block_0"), "CaveBlock"),
    MakeSolidTileArchetype(Tile::CaveShopWall, TileFamily::Cave,
                           HashFrameDataIdConstexpr("cave_shop_wall"), "CaveShopWall"),
    MakeSolidTileArchetype(Tile::CaveSmoothWall, TileFamily::Cave,
                           HashFrameDataIdConstexpr("cave_smooth_wall"), "CaveSmoothWall"),
    MakeSolidTileArchetype(Tile::Glass, TileFamily::Neutral, HashFrameDataIdConstexpr("glass"),
                           "Glass"),
    MakeSolidTileArchetype(Tile::LawsonWall, TileFamily::Cave,
                           HashFrameDataIdConstexpr("lawson_wall"), "LawsonWall"),
    TileArchetype{
        .tile = Tile::LawsonInside,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .family = TileFamily::Cave,
        .debug_name = "LawsonInside",
    },
    MakeSolidTileArchetype(Tile::LawsonLeftTopper, TileFamily::Cave,
                           HashFrameDataIdConstexpr("lawson_left_topper"), "LawsonLeftTopper"),
    MakeSolidTileArchetype(Tile::LawsonMiddleTopper, TileFamily::Cave,
                           HashFrameDataIdConstexpr("lawson_middle_topper"), "LawsonMiddleTopper"),
    MakeSolidTileArchetype(Tile::LawsonRightTopper, TileFamily::Cave,
                           HashFrameDataIdConstexpr("lawson_right_topper"), "LawsonRightTopper"),
    MakeSolidTileArchetype(Tile::LawsonFloor, TileFamily::Cave,
                           HashFrameDataIdConstexpr("lawson_floor"), "LawsonFloor"),
    TileArchetype{
        .tile = Tile::IceAir0,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .family = TileFamily::Ice,
        .debug_name = "IceAir0",
    },
    TileArchetype{
        .tile = Tile::IceAir1,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .family = TileFamily::Ice,
        .debug_name = "IceAir1",
    },
    TileArchetype{
        .tile = Tile::IceAir2,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .family = TileFamily::Ice,
        .debug_name = "IceAir2",
    },
    MakeSolidTileArchetype(Tile::IceDirt, TileFamily::Ice, HashFrameDataIdConstexpr("ice_dirt_0"),
                           "IceDirt", audio_asset_ids::Thud, nullptr, 1.0F, false),
    MakeSolidTileArchetype(Tile::IceBlock, TileFamily::Ice, HashFrameDataIdConstexpr("ice_block_0"),
                           "IceBlock", audio_asset_ids::Thud, nullptr, 1.0F, false),
    TileArchetype{
        .tile = Tile::JungleAir0,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .family = TileFamily::Jungle,
        .debug_name = "JungleAir0",
    },
    TileArchetype{
        .tile = Tile::JungleAir1,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .family = TileFamily::Jungle,
        .debug_name = "JungleAir1",
    },
    TileArchetype{
        .tile = Tile::JungleAir2,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .family = TileFamily::Jungle,
        .debug_name = "JungleAir2",
    },
    MakeSolidTileArchetype(Tile::JungleDirt, TileFamily::Jungle,
                           HashFrameDataIdConstexpr("jungle_dirt_0"), "JungleDirt"),
    MakeSolidTileArchetype(Tile::JungleBlock, TileFamily::Jungle,
                           HashFrameDataIdConstexpr("jungle_block_0"), "JungleBlock"),
    TileArchetype{
        .tile = Tile::TempleAir0,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .family = TileFamily::Temple,
        .debug_name = "TempleAir0",
    },
    TileArchetype{
        .tile = Tile::TempleAir1,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .family = TileFamily::Temple,
        .debug_name = "TempleAir1",
    },
    TileArchetype{
        .tile = Tile::TempleAir2,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .family = TileFamily::Temple,
        .debug_name = "TempleAir2",
    },
    MakeSolidTileArchetype(Tile::TempleDirt, TileFamily::Temple,
                           HashFrameDataIdConstexpr("temple_dirt_0"), "TempleDirt"),
    MakeSolidTileArchetype(Tile::TempleGold, TileFamily::Temple,
                           HashFrameDataIdConstexpr("temple_gold"), "TempleGold",
                           audio_asset_ids::MoneySmashed, OnBreakAsBigGoldMaterial),
    MakeSolidTileArchetype(Tile::TempleBlock, TileFamily::Temple,
                           HashFrameDataIdConstexpr("temple_block_0"), "TempleBlock"),
    TileArchetype{
        .tile = Tile::BossAir0,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .family = TileFamily::Boss,
        .debug_name = "BossAir0",
    },
    TileArchetype{
        .tile = Tile::BossAir1,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .family = TileFamily::Boss,
        .debug_name = "BossAir1",
    },
    TileArchetype{
        .tile = Tile::BossAir2,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .family = TileFamily::Boss,
        .debug_name = "BossAir2",
    },
    MakeSolidTileArchetype(Tile::BossDirt, TileFamily::Boss,
                           HashFrameDataIdConstexpr("boss_dirt_0"), "BossDirt"),
    MakeSolidTileArchetype(Tile::BossBlock, TileFamily::Boss,
                           HashFrameDataIdConstexpr("boss_block_0"), "BossBlock"),
    TileArchetype{
        .tile = Tile::LadderTop,
        .solid = false,
        .climbable = true,
        .transparent = true,
        .hangable = false,
        .debug_name = "LadderTop",
    },
    TileArchetype{
        .tile = Tile::Ladder,
        .solid = false,
        .climbable = true,
        .transparent = true,
        .hangable = false,
        .debug_name = "Ladder",
    },
    TileArchetype{
        .tile = Tile::LadderOrange,
        .solid = false,
        .climbable = true,
        .transparent = true,
        .hangable = false,
        .debug_name = "LadderOrange",
    },
    TileArchetype{
        .tile = Tile::Spikes,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .debug_name = "Spikes",
    },
    TileArchetype{
        .tile = Tile::Rope,
        .solid = false,
        .climbable = true,
        .transparent = true,
        .hangable = false,
        .debug_name = "Rope",
    },
    TileArchetype{
        .tile = Tile::Vine,
        .solid = false,
        .climbable = true,
        .transparent = true,
        .hangable = false,
        .family = TileFamily::Jungle,
        .debug_name = "Vine",
    },
    TileArchetype{
        .tile = Tile::VineTop,
        .solid = false,
        .climbable = true,
        .transparent = true,
        .hangable = false,
        .family = TileFamily::Jungle,
        .debug_name = "VineTop",
    },
    TileArchetype{
        .tile = Tile::WaterSwim,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .debug_name = "WaterSwim",
    },
    TileArchetype{
        .tile = Tile::Lava,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .family = TileFamily::Temple,
        .debug_name = "Lava",
    },
    MakeSolidTileArchetype(Tile::Lush, TileFamily::Jungle,
                           HashFrameDataIdConstexpr("jungle_dirt_0"), "Lush"),
    TileArchetype{
        .tile = Tile::Tree,
        .solid = false,
        .climbable = true,
        .transparent = true,
        .hangable = false,
        .family = TileFamily::Jungle,
        .debug_name = "Tree",
    },
    MakeSolidTileArchetype(Tile::ThinIce, TileFamily::Ice, HashFrameDataIdConstexpr("ice_block_0"),
                           "ThinIce", audio_asset_ids::Thud, nullptr, 1.0F, false),
    MakeSolidTileArchetype(Tile::Dark, TileFamily::Ice, HashFrameDataIdConstexpr("ice_dirt_0"),
                           "Dark", audio_asset_ids::Thud, nullptr, 1.0F, false),
    MakeSolidTileArchetype(Tile::DarkFall, TileFamily::Ice, HashFrameDataIdConstexpr("ice_block_0"),
                           "DarkFall", audio_asset_ids::Thud, nullptr, 1.0F, false),
    MakeSolidTileArchetype(Tile::AlienShip, TileFamily::Ice,
                           HashFrameDataIdConstexpr("ice_block_0"), "AlienShip",
                           audio_asset_ids::Thud, nullptr, 1.0F, false),
    MakeSolidTileArchetype(Tile::TempleFake, TileFamily::Temple,
                           HashFrameDataIdConstexpr("temple_dirt_0"), "TempleFake"),
    TileArchetype{
        .tile = Tile::Entrance,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .debug_name = "Entrance",
    },
    TileArchetype{
        .tile = Tile::Exit,
        .solid = false,
        .climbable = false,
        .transparent = true,
        .hangable = false,
        .debug_name = "Exit",
    },
}};

} // namespace

const TileArchetype& GetTileArchetype(Tile tile) {
    const std::size_t index = TileIndex(tile);
    if (index >= kTileCount) {
        throw std::runtime_error("GetTileArchetype received invalid tile");
    }
    return kTileArchetypes[index];
}

bool IsTileTransparent(Tile tile) {
    return GetTileArchetype(tile).transparent;
}

bool IsTileCollidable(Tile tile) {
    return GetTileArchetype(tile).solid;
}

bool IsTileHangable(Tile tile) {
    return GetTileArchetype(tile).hangable;
}

float GetTileFriction(Tile tile) {
    return GetTileArchetype(tile).friction;
}

} // namespace splonks

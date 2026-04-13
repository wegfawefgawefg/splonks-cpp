#include "tile.hpp"

#include "stage.hpp"
#include "tile_archetype.hpp"
#include "utils.hpp"

namespace splonks {

namespace {

struct TileFamilyVariants {
    Tile dirt;
    Tile gold;
    Tile gold_big;
    Tile block;
    Tile shop_wall;
    Tile smooth_wall;
    Tile glass;
};

const TileFamilyVariants& GetTileFamilyVariants(TileFamily family) {
    static constexpr TileFamilyVariants kNeutralVariants{
        .dirt = Tile::CaveDirt,
        .gold = Tile::CaveGold,
        .gold_big = Tile::CaveGoldBig,
        .block = Tile::CaveBlock,
        .shop_wall = Tile::CaveShopWall,
        .smooth_wall = Tile::CaveSmoothWall,
        .glass = Tile::Glass,
    };
    static constexpr TileFamilyVariants kCaveVariants{
        .dirt = Tile::CaveDirt,
        .gold = Tile::CaveGold,
        .gold_big = Tile::CaveGoldBig,
        .block = Tile::CaveBlock,
        .shop_wall = Tile::CaveShopWall,
        .smooth_wall = Tile::CaveSmoothWall,
        .glass = Tile::Glass,
    };
    static constexpr TileFamilyVariants kIceVariants{
        .dirt = Tile::IceDirt,
        .gold = Tile::IceGold,
        .gold_big = Tile::IceGoldBig,
        .block = Tile::IceBlock,
        .shop_wall = Tile::CaveShopWall,
        .smooth_wall = Tile::CaveSmoothWall,
        .glass = Tile::Glass,
    };
    static constexpr TileFamilyVariants kJungleVariants{
        .dirt = Tile::JungleDirt,
        .gold = Tile::JungleGold,
        .gold_big = Tile::JungleGoldBig,
        .block = Tile::JungleBlock,
        .shop_wall = Tile::CaveShopWall,
        .smooth_wall = Tile::CaveSmoothWall,
        .glass = Tile::Glass,
    };
    static constexpr TileFamilyVariants kTempleVariants{
        .dirt = Tile::TempleDirt,
        .gold = Tile::TempleGold,
        .gold_big = Tile::TempleGoldBig,
        .block = Tile::TempleBlock,
        .shop_wall = Tile::CaveShopWall,
        .smooth_wall = Tile::CaveSmoothWall,
        .glass = Tile::Glass,
    };
    static constexpr TileFamilyVariants kBossVariants{
        .dirt = Tile::BossDirt,
        .gold = Tile::BossGold,
        .gold_big = Tile::BossGoldBig,
        .block = Tile::BossBlock,
        .shop_wall = Tile::CaveShopWall,
        .smooth_wall = Tile::CaveSmoothWall,
        .glass = Tile::Glass,
    };

    switch (family) {
    case TileFamily::Cave:
        return kCaveVariants;
    case TileFamily::Ice:
        return kIceVariants;
    case TileFamily::Jungle:
        return kJungleVariants;
    case TileFamily::Temple:
        return kTempleVariants;
    case TileFamily::Boss:
        return kBossVariants;
    case TileFamily::Neutral:
        return kNeutralVariants;
    }

    return kNeutralVariants;
}

const TileFamilyVariants& GetTileFamilyVariantsForTile(Tile tile) {
    return GetTileFamilyVariants(GetTileArchetype(tile).family);
}

} // namespace

Tile RandomTile() {
    const int tile_index = rng::RandomIntInclusive(0, static_cast<int>(kTileCount) - 1);
    return static_cast<Tile>(tile_index);
}

Tile DirtTileForFamilyTile(Tile family_tile) {
    return GetTileFamilyVariantsForTile(family_tile).dirt;
}

Tile GoldTileForFamilyTile(Tile family_tile) {
    return GetTileFamilyVariantsForTile(family_tile).gold;
}

Tile GoldBigTileForFamilyTile(Tile family_tile) {
    return GetTileFamilyVariantsForTile(family_tile).gold_big;
}

Tile BlockTileForFamilyTile(Tile family_tile) {
    return GetTileFamilyVariantsForTile(family_tile).block;
}

Tile ShopWallTileForFamilyTile(Tile family_tile) {
    return GetTileFamilyVariantsForTile(family_tile).shop_wall;
}

Tile SmoothWallTileForFamilyTile(Tile family_tile) {
    return GetTileFamilyVariantsForTile(family_tile).smooth_wall;
}

Tile GlassTileForFamilyTile(Tile family_tile) {
    return GetTileFamilyVariantsForTile(family_tile).glass;
}

const char* TileToString(Tile tile) {
    return GetTileArchetype(tile).debug_name;
}

bool IsDirtTile(Tile tile) {
    switch (tile) {
    case Tile::CaveDirt:
    case Tile::IceDirt:
    case Tile::JungleDirt:
    case Tile::TempleDirt:
    case Tile::BossDirt:
        return true;
    default:
        return false;
    }
}

bool CollidableTileInList(const std::vector<const Tile*>& tiles) {
    for (const Tile* tile : tiles) {
        if (IsTileCollidable(*tile)) {
            return true;
        }
    }
    return false;
}

bool ClimbableTileInList(const std::vector<const Tile*>& tiles) {
    for (const Tile* tile : tiles) {
        if (GetTileArchetype(*tile).climbable) {
            return true;
        }
    }
    return false;
}

bool HangableTileInList(const std::vector<const Tile*>& tiles) {
    for (const Tile* tile : tiles) {
        if (GetTileArchetype(*tile).hangable) {
            return true;
        }
    }
    return false;
}

} // namespace splonks

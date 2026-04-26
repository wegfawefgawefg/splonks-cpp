#include "stage_gen/classic/tile_palette.hpp"

namespace splonks::stage_gen::classic {

namespace {

enum class TilePaletteFamily {
    Neutral,
    Cave,
    Ice,
    Jungle,
    Temple,
    Boss,
};

struct TileFamilyVariants {
    Tile dirt;
    Tile block;
    Tile shop_wall;
    Tile smooth_wall;
};

TilePaletteFamily GetTilePaletteFamilyForTile(Tile tile) {
    switch (tile) {
    case Tile::IceAir0:
    case Tile::IceAir1:
    case Tile::IceAir2:
    case Tile::IceDirt:
    case Tile::IceBlock:
    case Tile::ThinIce:
    case Tile::Dark:
    case Tile::DarkFall:
    case Tile::AlienShip:
        return TilePaletteFamily::Ice;
    case Tile::JungleAir0:
    case Tile::JungleAir1:
    case Tile::JungleAir2:
    case Tile::JungleDirt:
    case Tile::JungleBlock:
    case Tile::Lush:
    case Tile::Tree:
    case Tile::Vine:
    case Tile::VineTop:
        return TilePaletteFamily::Jungle;
    case Tile::TempleAir0:
    case Tile::TempleAir1:
    case Tile::TempleAir2:
    case Tile::TempleDirt:
    case Tile::TempleGold:
    case Tile::TempleBlock:
    case Tile::TempleFake:
        return TilePaletteFamily::Temple;
    case Tile::BossAir0:
    case Tile::BossAir1:
    case Tile::BossAir2:
    case Tile::BossDirt:
    case Tile::BossBlock:
        return TilePaletteFamily::Boss;
    case Tile::CaveAir0:
    case Tile::CaveAir1:
    case Tile::CaveAir2:
    case Tile::CaveDirt:
    case Tile::CaveBlock:
    case Tile::CaveShopWall:
    case Tile::CaveSmoothWall:
    case Tile::LawsonWall:
    case Tile::LawsonInside:
    case Tile::LawsonLeftTopper:
    case Tile::LawsonMiddleTopper:
    case Tile::LawsonRightTopper:
    case Tile::LawsonFloor:
        return TilePaletteFamily::Cave;
    default:
        return TilePaletteFamily::Neutral;
    }
}

const TileFamilyVariants& GetTileFamilyVariants(TilePaletteFamily family) {
    static constexpr TileFamilyVariants kNeutralVariants{
        .dirt = Tile::CaveDirt,
        .block = Tile::CaveBlock,
        .shop_wall = Tile::CaveShopWall,
        .smooth_wall = Tile::CaveSmoothWall,
    };
    static constexpr TileFamilyVariants kCaveVariants{
        .dirt = Tile::CaveDirt,
        .block = Tile::CaveBlock,
        .shop_wall = Tile::CaveShopWall,
        .smooth_wall = Tile::CaveSmoothWall,
    };
    static constexpr TileFamilyVariants kIceVariants{
        .dirt = Tile::IceDirt,
        .block = Tile::IceBlock,
        .shop_wall = Tile::CaveShopWall,
        .smooth_wall = Tile::CaveSmoothWall,
    };
    static constexpr TileFamilyVariants kJungleVariants{
        .dirt = Tile::JungleDirt,
        .block = Tile::JungleBlock,
        .shop_wall = Tile::CaveShopWall,
        .smooth_wall = Tile::CaveSmoothWall,
    };
    static constexpr TileFamilyVariants kTempleVariants{
        .dirt = Tile::TempleDirt,
        .block = Tile::TempleBlock,
        .shop_wall = Tile::CaveShopWall,
        .smooth_wall = Tile::CaveSmoothWall,
    };
    static constexpr TileFamilyVariants kBossVariants{
        .dirt = Tile::BossDirt,
        .block = Tile::BossBlock,
        .shop_wall = Tile::CaveShopWall,
        .smooth_wall = Tile::CaveSmoothWall,
    };

    switch (family) {
    case TilePaletteFamily::Cave:
        return kCaveVariants;
    case TilePaletteFamily::Ice:
        return kIceVariants;
    case TilePaletteFamily::Jungle:
        return kJungleVariants;
    case TilePaletteFamily::Temple:
        return kTempleVariants;
    case TilePaletteFamily::Boss:
        return kBossVariants;
    case TilePaletteFamily::Neutral:
        return kNeutralVariants;
    }

    return kNeutralVariants;
}

const TileFamilyVariants& GetTileFamilyVariantsForTile(Tile tile) {
    return GetTileFamilyVariants(GetTilePaletteFamilyForTile(tile));
}

} // namespace

Tile DirtTileForFamilyTile(Tile family_tile) {
    return GetTileFamilyVariantsForTile(family_tile).dirt;
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

bool IsBlockTile(Tile tile) {
    switch (tile) {
    case Tile::CaveBlock:
    case Tile::IceBlock:
    case Tile::JungleBlock:
    case Tile::TempleBlock:
    case Tile::BossBlock:
        return true;
    default:
        return false;
    }
}

} // namespace splonks::stage_gen::classic

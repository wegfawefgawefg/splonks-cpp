#pragma once

#include <cstddef>
#include <vector>

namespace splonks {

enum class StageType : int;

enum class Tile {
    Air,
    CaveAir0,
    CaveAir1,
    CaveAir2,
    CaveDirt,
    CaveGold,
    CaveGoldBig,
    CaveBlock,
    CaveShopWall,
    CaveSmoothWall,
    Glass,
    LawsonWall,
    LawsonInside,
    LawsonLeftTopper,
    LawsonMiddleTopper,
    LawsonRightTopper,
    LawsonFloor,
    IceAir0,
    IceAir1,
    IceAir2,
    IceDirt,
    IceGold,
    IceGoldBig,
    IceBlock,
    JungleAir0,
    JungleAir1,
    JungleAir2,
    JungleDirt,
    JungleGold,
    JungleGoldBig,
    JungleBlock,
    TempleAir0,
    TempleAir1,
    TempleAir2,
    TempleDirt,
    TempleGold,
    TempleGoldBig,
    TempleBlock,
    BossAir0,
    BossAir1,
    BossAir2,
    BossDirt,
    BossGold,
    BossGoldBig,
    BossBlock,
    LadderTop,
    Ladder,
    Spikes,
    Rope,
    Entrance,
    Exit,
};

constexpr unsigned int kTileSize = 16;
constexpr std::size_t kTileCount = 51;

Tile RandomTile();
Tile DirtTileForFamilyTile(Tile family_tile);
Tile GoldTileForFamilyTile(Tile family_tile);
Tile GoldBigTileForFamilyTile(Tile family_tile);
Tile BlockTileForFamilyTile(Tile family_tile);
Tile ShopWallTileForFamilyTile(Tile family_tile);
Tile SmoothWallTileForFamilyTile(Tile family_tile);
Tile GlassTileForFamilyTile(Tile family_tile);
const char* TileToString(Tile tile);
bool IsDirtTile(Tile tile);
bool IsTileTransparent(Tile tile);
bool IsTileCollidable(Tile tile);
bool IsTileHangable(Tile tile);
bool CollidableTileInList(const std::vector<const Tile*>& tiles);
bool ClimbableTileInList(const std::vector<const Tile*>& tiles);
bool HangableTileInList(const std::vector<const Tile*>& tiles);

} // namespace splonks

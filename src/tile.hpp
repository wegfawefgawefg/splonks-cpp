#pragma once

#include <cstddef>
#include <vector>

namespace splonks {

enum class StageType : int;

enum class Tile {
    Air,
    CaveDirt,
    CaveGold,
    CaveGoldBig,
    CaveBlock,
    CaveShopWall,
    CaveSmoothWall,
    Glass,
    IceDirt,
    IceGold,
    IceGoldBig,
    IceBlock,
    JungleDirt,
    JungleGold,
    JungleGoldBig,
    JungleBlock,
    TempleDirt,
    TempleGold,
    TempleGoldBig,
    TempleBlock,
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
constexpr std::size_t kTileCount = 30;

Tile RandomTile();
Tile DirtTileForFamilyTile(Tile family_tile);
Tile GoldTileForFamilyTile(Tile family_tile);
Tile GoldBigTileForFamilyTile(Tile family_tile);
Tile BlockTileForFamilyTile(Tile family_tile);
Tile ShopWallTileForFamilyTile(Tile family_tile);
Tile SmoothWallTileForFamilyTile(Tile family_tile);
Tile GlassTileForFamilyTile(Tile family_tile);
Tile BorderTileForStageType(StageType stage_type);
bool IsDirtTile(Tile tile);
bool IsTileTransparent(Tile tile);
bool IsTileCollidable(Tile tile);
bool IsTileHangable(Tile tile);
bool CollidableTileInList(const std::vector<const Tile*>& tiles);
bool ClimbableTileInList(const std::vector<const Tile*>& tiles);
bool HangableTileInList(const std::vector<const Tile*>& tiles);

} // namespace splonks

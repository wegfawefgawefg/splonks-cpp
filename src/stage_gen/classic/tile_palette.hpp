#pragma once

#include "tile.hpp"

namespace splonks::stage_gen::classic {

Tile DirtTileForFamilyTile(Tile family_tile);
Tile BlockTileForFamilyTile(Tile family_tile);
Tile ShopWallTileForFamilyTile(Tile family_tile);
Tile SmoothWallTileForFamilyTile(Tile family_tile);
bool IsBlockTile(Tile tile);

} // namespace splonks::stage_gen::classic

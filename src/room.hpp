#pragma once

#include "ent.hpp"
#include "math_types.hpp"
#include "tile.hpp"

#include <vector>

namespace splonks {

enum class StageType : int;

enum class RoomType {
    Entrance,
    Box,
    LeftRight,
    LeftUpRight,
    LeftDownRight,
    FourWay,
    Exit,
};

enum class MetaTile {
    Solid,
    Air,
    MaybeSolid,
    Ladder,
    LadderTop,
    MaybeSpikes,
    MaybeBlock,
    Entrance,
    Exit,
};

struct Room {
    std::vector<std::vector<Tile>> tiles;
    std::vector<Ent> ents;
};

struct RoomTilePalette {
    Tile dirt = Tile::CaveDirt;
    Tile block = Tile::CaveBlock;
};

RoomType RandomRoomType(DetRng& det_rng);
std::vector<std::vector<Tile>> GenRoom(RoomType room_type, StageType stage_type,
                                       RoomTilePalette tile_palette, DetRng& det_rng);
void PasteTemplate(std::vector<std::vector<MetaTile>>& parent,
                   const std::vector<std::vector<MetaTile>>& child, const UVec2& location,
                   bool flip_horizontal, bool flip_vertical);
std::vector<std::vector<Tile>> ResolveRoomTemplate(
    const std::vector<std::vector<MetaTile>>& room_template,
    RoomTilePalette tile_palette, DetRng& det_rng);

} // namespace splonks

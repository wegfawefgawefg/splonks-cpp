#pragma once

#include "entity.hpp"
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

enum class TemplateTile {
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
    std::vector<Entity> entities;
};

RoomType RandomRoomType();
std::vector<std::vector<Tile>> GenRoom(RoomType room_type, StageType stage_type, Tile family_tile);
void PasteTemplate(std::vector<std::vector<TemplateTile>>& parent,
                   const std::vector<std::vector<TemplateTile>>& child, const UVec2& location,
                   bool flip_horizontal, bool flip_vertical);
std::vector<std::vector<Tile>> ResolveRoomTemplate(
    const std::vector<std::vector<TemplateTile>>& room_template,
    Tile family_tile);

} // namespace splonks

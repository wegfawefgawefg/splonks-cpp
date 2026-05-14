#include "room.hpp"

#include "stage.hpp"
#include "stage_gen/cave.hpp"
#include "stage_gen/test.hpp"

#include <stdexcept>

namespace splonks {

namespace {

} // namespace

RoomType RandomRoomType(DetRng& det_rng) {
    switch (det_rng.RandomIntInclusive(0, 5)) {
    case 0:
        return RoomType::Box;
    case 1:
        return RoomType::LeftRight;
    case 2:
        return RoomType::LeftUpRight;
    case 3:
        return RoomType::LeftDownRight;
    case 4:
        return RoomType::FourWay;
    case 5:
        return RoomType::Exit;
    default:
        throw std::runtime_error("RandomRoomType generated unreachable room type");
    }
}

std::vector<std::vector<Tile>> GenRoom(RoomType room_type, StageType stage_type,
                                       RoomTilePalette tile_palette, DetRng& det_rng) {
    std::vector<std::vector<MetaTile>> room_template;
    switch (stage_type) {
    case StageType::Test1:
        room_template = stage_gen::test::GetRoomTemplate(room_type, det_rng);
        break;
    default:
        room_template = stage_gen::cave::GetRoomTemplate(room_type, det_rng);
        break;
    }

    return ResolveRoomTemplate(room_template, tile_palette, det_rng);
}

void PasteTemplate(std::vector<std::vector<MetaTile>>& parent,
                   const std::vector<std::vector<MetaTile>>& child, const UVec2& location,
                   bool flip_horizontal, bool flip_vertical) {
    for (std::size_t child_y = 0; child_y < child.size(); ++child_y) {
        for (std::size_t child_x = 0; child_x < child[0].size(); ++child_x) {
            const unsigned int parent_x = location.x + static_cast<unsigned int>(child_x);
            const unsigned int parent_y = location.y + static_cast<unsigned int>(child_y);
            std::size_t sample_x = child_x;
            std::size_t sample_y = child_y;
            if (flip_vertical) {
                sample_y = child.size() - sample_y - 1;
            }
            if (flip_horizontal) {
                sample_x = child[0].size() - sample_x - 1;
            }
            parent[static_cast<std::size_t>(parent_y)][static_cast<std::size_t>(parent_x)] =
                child[sample_y][sample_x];
        }
    }
}

std::vector<std::vector<Tile>> ResolveRoomTemplate(
    const std::vector<std::vector<MetaTile>>& meta_tiles,
    RoomTilePalette tile_palette, DetRng& det_rng) {
    std::vector<std::vector<Tile>> room(
        static_cast<std::size_t>(Stage::kRoomShape.y),
        std::vector<Tile>(static_cast<std::size_t>(Stage::kRoomShape.x), Tile::Air));

    for (unsigned int y = 0; y < Stage::kRoomShape.y; ++y) {
        for (unsigned int x = 0; x < Stage::kRoomShape.x; ++x) {
            const MetaTile meta_tile =
                meta_tiles[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];

            Tile tile_type = Tile::Air;
            switch (meta_tile) {
            case MetaTile::Solid: {
                const int chance = det_rng.RandomIntInclusive(0, 99);
                if (chance < 10) {
                    tile_type = tile_palette.dirt;
                } else if (chance < 20) {
                    tile_type = tile_palette.block;
                } else {
                    tile_type = tile_palette.dirt;
                }
                break;
            }
            case MetaTile::Air:
                tile_type = Tile::Air;
                break;
            case MetaTile::Ladder:
                tile_type = Tile::Ladder;
                break;
            case MetaTile::LadderTop:
                tile_type = Tile::LadderTop;
                break;
            case MetaTile::MaybeSolid: {
                const int chance = det_rng.RandomIntInclusive(0, 99);
                tile_type = chance < 50 ? tile_palette.dirt : Tile::Air;
                break;
            }
            case MetaTile::MaybeSpikes: {
                const int chance = det_rng.RandomIntInclusive(0, 99);
                tile_type = chance < 50 ? Tile::Spikes : Tile::Air;
                break;
            }
            case MetaTile::MaybeBlock: {
                const int chance = det_rng.RandomIntInclusive(0, 99);
                tile_type = chance < 50 ? Tile::Air : tile_palette.block;
                break;
            }
            case MetaTile::Entrance:
                tile_type = Tile::Entrance;
                break;
            case MetaTile::Exit:
                tile_type = Tile::Exit;
                break;
            }

            room[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = tile_type;
        }
    }

    return room;
}

} // namespace splonks

#include "room.hpp"

#include "stage.hpp"
#include "stage_gen/cave.hpp"
#include "stage_gen/test.hpp"

#include <stdexcept>

namespace splonks {

namespace {

} // namespace

RoomType RandomRoomType() {
    switch (rng::RandomIntInclusive(0, 5)) {
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

std::vector<std::vector<Tile>> GenRoom(RoomType room_type, StageType stage_type) {
    std::vector<std::vector<TemplateTile>> room_template;
    switch (stage_type) {
    case StageType::Test1:
        room_template = stage_gen::test::GetRoomTemplate(room_type);
        break;
    case StageType::Cave1:
        room_template = stage_gen::cave::GetRoomTemplate(room_type);
        break;
    default:
        room_template = stage_gen::cave::GetRoomTemplate(room_type);
        break;
    }

    return ResolveRoomTemplate(room_template);
}

void PasteTemplate(std::vector<std::vector<TemplateTile>>& parent,
                   const std::vector<std::vector<TemplateTile>>& child, const UVec2& location,
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
    const std::vector<std::vector<TemplateTile>>& template_tiles) {
    std::vector<std::vector<Tile>> room(
        static_cast<std::size_t>(Stage::kRoomShape.y),
        std::vector<Tile>(static_cast<std::size_t>(Stage::kRoomShape.x), Tile::Air));

    for (unsigned int y = 0; y < Stage::kRoomShape.y; ++y) {
        for (unsigned int x = 0; x < Stage::kRoomShape.x; ++x) {
            const TemplateTile template_tile =
                template_tiles[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];

            Tile tile_type = Tile::Air;
            switch (template_tile) {
            case TemplateTile::Solid: {
                const int chance = rng::RandomIntInclusive(0, 99);
                if (chance < 10) {
                    tile_type = Tile::Gold;
                } else if (chance < 20) {
                    tile_type = Tile::Block;
                } else {
                    tile_type = Tile::Dirt;
                }
                break;
            }
            case TemplateTile::Air:
                tile_type = Tile::Air;
                break;
            case TemplateTile::Ladder:
                tile_type = Tile::Ladder;
                break;
            case TemplateTile::LadderTop:
                tile_type = Tile::LadderTop;
                break;
            case TemplateTile::MaybeSolid: {
                const int chance = rng::RandomIntInclusive(0, 99);
                tile_type = chance < 50 ? Tile::Dirt : Tile::Air;
                break;
            }
            case TemplateTile::MaybeSpikes: {
                const int chance = rng::RandomIntInclusive(0, 99);
                tile_type = chance < 50 ? Tile::Spikes : Tile::Air;
                break;
            }
            case TemplateTile::MaybeBlock: {
                const int chance = rng::RandomIntInclusive(0, 99);
                tile_type = chance < 50 ? Tile::Air : Tile::Block;
                break;
            }
            case TemplateTile::Entrance:
                tile_type = Tile::Entrance;
                break;
            case TemplateTile::Exit:
                tile_type = Tile::Exit;
                break;
            }

            room[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = tile_type;
        }
    }

    return room;
}

} // namespace splonks

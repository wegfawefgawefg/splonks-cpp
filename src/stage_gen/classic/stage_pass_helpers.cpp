#include "stage_gen/classic/stage_pass_helpers.hpp"

#include "stage_gen/classic/room_layout.hpp"
#include "utils.hpp"

#include <algorithm>
#include <limits>
#include <vector>

namespace splonks::stage_gen::classic {

int PickStagePassIndex(std::size_t size) {
    return rng::RandomIntInclusive(0, static_cast<int>(size) - 1);
}

std::string GetPassString(const StagePassConfig& pass, std::string_view key,
                          std::string_view fallback) {
    const auto it = pass.properties.find(std::string(key));
    if (it == pass.properties.end()) {
        return std::string(fallback);
    }
    return it->second;
}

bool TileCoordExists(const Stage& stage, int tile_x, int tile_y) {
    return tile_x >= 0 && tile_y >= 0 && tile_x < static_cast<int>(stage.GetTileWidth()) &&
           tile_y < static_cast<int>(stage.GetTileHeight());
}

bool IsCollidableTileAt(const Stage& stage, int tile_x, int tile_y) {
    if (!TileCoordExists(stage, tile_x, tile_y)) {
        return false;
    }
    return IsTileCollidable(
        stage.GetTile(static_cast<unsigned int>(tile_x), static_cast<unsigned int>(tile_y)));
}

IVec2 GetRoomAtTileCoord(const Stage& stage, int tile_x, int tile_y) {
    const UVec2 room_layout_dims = stage.GetRoomLayoutDims();
    if (room_layout_dims.x == 0 || room_layout_dims.y == 0) {
        return IVec2::New(0, 0);
    }
    const int room_width = std::max(1, static_cast<int>(stage.GetTileWidth() / room_layout_dims.x));
    const int room_height = std::max(1, static_cast<int>(stage.GetTileHeight() / room_layout_dims.y));
    return IVec2::New(tile_x / room_width, tile_y / room_height);
}

bool IsShopRoomCode(int room_code) {
    return room_code == static_cast<int>(RoomCode::ShopLeft) ||
           room_code == static_cast<int>(RoomCode::ShopRight);
}

bool IsShopRoomAt(const Stage& stage, int tile_x, int tile_y) {
    const IVec2 room = GetRoomAtTileCoord(stage, tile_x, tile_y);
    const UVec2 room_layout_dims = stage.GetRoomLayoutDims();
    if (room.x < 0 || room.y < 0 || room.x >= static_cast<int>(room_layout_dims.x) ||
        room.y >= static_cast<int>(room_layout_dims.y)) {
        return false;
    }
    return IsShopRoomCode(
        stage.rooms[static_cast<std::size_t>(room.y)][static_cast<std::size_t>(room.x)]);
}

bool IsStartRoomAt(const Stage& stage, int tile_x, int tile_y) {
    return GetRoomAtTileCoord(stage, tile_x, tile_y) == stage.GetStartingRoom();
}

bool HasSpawnAtWorldPos(const Stage& stage, const Vec2& pos) {
    for (const EntSpawn& spawn : stage.ent_spawns) {
        if (spawn.pos == pos) {
            return true;
        }
    }
    return false;
}

std::optional<Vec2> FindEntrancePos(const Stage& stage) {
    for (unsigned int y = 0; y < stage.GetTileHeight(); ++y) {
        for (unsigned int x = 0; x < stage.GetTileWidth(); ++x) {
            if (stage.GetTile(x, y) != Tile::Entrance) {
                continue;
            }
            return Vec2::New(static_cast<float>(x * kTileSize), static_cast<float>(y * kTileSize));
        }
    }
    return std::nullopt;
}

std::optional<Vec2> FindExitPos(const Stage& stage) {
    for (const EntSpawn& spawn : stage.ent_spawns) {
        if (spawn.type_ == EntType::BasicExit) {
            return spawn.pos;
        }
    }
    return std::nullopt;
}

bool HasSpawnType(const Stage& stage, EntType type_) {
    for (const EntSpawn& spawn : stage.ent_spawns) {
        if (spawn.type_ == type_) {
            return true;
        }
    }
    return false;
}

float DistanceToNearestSpawnType(const Stage& stage, EntType type_, const Vec2& pos) {
    float nearest = std::numeric_limits<float>::infinity();
    for (const EntSpawn& spawn : stage.ent_spawns) {
        if (spawn.type_ != type_) {
            continue;
        }
        nearest = std::min(nearest, Length(spawn.pos - pos));
    }
    return nearest;
}

bool HasSpawnType(const std::vector<EntSpawn>& spawns, EntType type_) {
    for (const EntSpawn& spawn : spawns) {
        if (spawn.type_ == type_) {
            return true;
        }
    }
    return false;
}

bool HasSpawnType(const Stage& stage, const std::vector<EntSpawn>& spawns,
                  EntType type_) {
    return HasSpawnType(stage, type_) || HasSpawnType(spawns, type_);
}

bool HasExitSpawn(const Stage& stage, std::string_view exit_id) {
    for (const EntSpawn& spawn : stage.ent_spawns) {
        const std::string_view spawn_exit_id =
            spawn.exit_id.empty() ? std::string_view("default") : std::string_view(spawn.exit_id);
        if (spawn.type_ == EntType::BasicExit && spawn_exit_id == exit_id) {
            return true;
        }
    }
    return false;
}

void AddAmbientSpawn(Stage& stage, EntType type_, const Vec2& pos,
                     Side facing) {
    if (HasSpawnAtWorldPos(stage, pos)) {
        return;
    }
    stage.ent_spawns.push_back(EntSpawn{
        .type_ = type_,
        .pos = pos,
        .facing = facing,
        .exit_id = "",
    });
}

bool IsTreasureSpawnType(EntType type_) {
    switch (type_) {
    case EntType::Gold:
    case EntType::GoldStack:
    case EntType::EmeraldBig:
    case EntType::SapphireBig:
    case EntType::RubyBig:
        return true;
    default:
        return false;
    }
}

} // namespace splonks::stage_gen::classic

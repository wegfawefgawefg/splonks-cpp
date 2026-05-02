#pragma once

#include "frame_data.hpp"
#include "tile.hpp"

#include <cstdint>
#include <vector>

namespace splonks {

struct TileSourceDb;

struct TileContactData {
    FrameRect cbox;
};

struct TileContactSpan {
    std::uint32_t first_contact_index = 0;
    std::uint32_t contact_count = 0;
};

struct TileContactDb {
    std::vector<TileContactData> contacts;
    std::vector<TileContactSpan> tile_spans;
};

TileContactDb BuildTileContactDb(const TileSourceDb& tile_source_db);
const TileContactData* GetTileContactData(
    const TileContactDb& tile_contact_db,
    Tile tile,
    const IVec2& tile_pos
);

} // namespace splonks

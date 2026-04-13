#pragma once

#include "frame_data.hpp"
#include "stage.hpp"
#include "tile.hpp"

#include <array>
#include <cstdint>
#include <vector>

struct SDL_Texture;

namespace splonks {

struct Graphics;

enum class TileSet : int {
    Cave,
    Ice,
    Jungle,
    Temple,
    Boss,
};

constexpr std::size_t kTileSetCount = 5;

struct TileSourceData {
    std::uint32_t image_id = 0;
    FrameRect sample_rect;
};

struct TileSourceSpan {
    std::uint32_t first_source_index = 0;
    std::uint32_t source_count = 0;
};

struct TileSourceDb {
    std::vector<TileSourceData> sources;
    std::vector<TileSourceSpan> tile_spans;
    std::array<TileSourceSpan, kTileSetCount> air_spans{};
};

TileSourceDb BuildTileSourceDb(const FrameDataDb& frame_data_db);
TileSet TileSetForStageType(StageType stage_type);
const TileSourceData* GetTileSourceData(Graphics& graphics, Tile tile, const IVec2& tile_pos);
const TileSourceData* GetAirSourceData(Graphics& graphics, TileSet tile_set, const IVec2& tile_pos);
SDL_Texture* GetTileTexture(const Graphics& graphics, const TileSourceData& tile_source_data);

} // namespace splonks

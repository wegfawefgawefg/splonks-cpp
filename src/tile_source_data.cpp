#include "tile_source_data.hpp"

#include "graphics.hpp"
#include "water.hpp"

#include <array>
#include <stdexcept>

namespace splonks {

namespace {

constexpr std::size_t TileToIndex(Tile tile) {
    return static_cast<std::size_t>(tile);
}

std::uint64_t TileVariationCacheKey(const IVec2& tile_pos) {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(tile_pos.x)) << 32U) |
           static_cast<std::uint32_t>(tile_pos.y);
}

struct TileSourceNameGroup {
    Tile tile;
    std::array<std::uint32_t, 3> name_hashes{};
    std::uint32_t count = 0;
};

constexpr std::array<TileSourceNameGroup, 55> kTileSourceNameGroups{{
    {Tile::CaveAir0, {HashAFrameIdConstexpr("cave_air_0"), 0, 0}, 1},
    {Tile::CaveAir1, {HashAFrameIdConstexpr("cave_air_1"), 0, 0}, 1},
    {Tile::CaveAir2, {HashAFrameIdConstexpr("cave_air_2"), 0, 0}, 1},
    {Tile::CaveDirt,
     {HashAFrameIdConstexpr("cave_dirt_0"), HashAFrameIdConstexpr("cave_dirt_1"),
      HashAFrameIdConstexpr("cave_dirt_2")},
     3},
    {Tile::CaveBlock, {HashAFrameIdConstexpr("cave_block_0"), 0, 0}, 1},
    {Tile::CaveShopWall, {HashAFrameIdConstexpr("cave_shop_wall"), 0, 0}, 1},
    {Tile::CaveSmoothWall, {HashAFrameIdConstexpr("cave_smooth_wall"), 0, 0}, 1},
    {Tile::Glass, {HashAFrameIdConstexpr("glass"), 0, 0}, 1},
    {Tile::LawsonWall, {HashAFrameIdConstexpr("lawson_wall"), 0, 0}, 1},
    {Tile::LawsonInside, {HashAFrameIdConstexpr("lawson_inside"), 0, 0}, 1},
    {Tile::LawsonLeftTopper, {HashAFrameIdConstexpr("lawson_left_topper"), 0, 0}, 1},
    {Tile::LawsonMiddleTopper, {HashAFrameIdConstexpr("lawson_middle_topper"), 0, 0}, 1},
    {Tile::LawsonRightTopper, {HashAFrameIdConstexpr("lawson_right_topper"), 0, 0}, 1},
    {Tile::LawsonFloor, {HashAFrameIdConstexpr("lawson_floor"), 0, 0}, 1},

    {Tile::IceAir0, {HashAFrameIdConstexpr("ice_air_0"), 0, 0}, 1},
    {Tile::IceAir1, {HashAFrameIdConstexpr("ice_air_1"), 0, 0}, 1},
    {Tile::IceAir2, {HashAFrameIdConstexpr("ice_air_2"), 0, 0}, 1},
    {Tile::IceDirt,
     {HashAFrameIdConstexpr("ice_dirt_0"), HashAFrameIdConstexpr("ice_dirt_1"),
      HashAFrameIdConstexpr("ice_dirt_2")},
     3},
    {Tile::IceBlock, {HashAFrameIdConstexpr("ice_block_0"), 0, 0}, 1},

    {Tile::JungleAir0, {HashAFrameIdConstexpr("jungle_air_0"), 0, 0}, 1},
    {Tile::JungleAir1, {HashAFrameIdConstexpr("jungle_air_1"), 0, 0}, 1},
    {Tile::JungleAir2, {HashAFrameIdConstexpr("jungle_air_2"), 0, 0}, 1},
    {Tile::JungleDirt,
     {HashAFrameIdConstexpr("jungle_dirt_0"), HashAFrameIdConstexpr("jungle_dirt_1"),
      HashAFrameIdConstexpr("jungle_dirt_2")},
     3},
    {Tile::JungleBlock, {HashAFrameIdConstexpr("jungle_block_0"), 0, 0}, 1},

    {Tile::TempleAir0, {HashAFrameIdConstexpr("temple_air_0"), 0, 0}, 1},
    {Tile::TempleAir1, {HashAFrameIdConstexpr("temple_air_1"), 0, 0}, 1},
    {Tile::TempleAir2, {HashAFrameIdConstexpr("temple_air_2"), 0, 0}, 1},
    {Tile::TempleDirt,
     {HashAFrameIdConstexpr("temple_dirt_0"), HashAFrameIdConstexpr("temple_dirt_1"),
      HashAFrameIdConstexpr("temple_dirt_2")},
     3},
    {Tile::TempleGold, {HashAFrameIdConstexpr("temple_gold"), 0, 0}, 1},
    {Tile::TempleBlock, {HashAFrameIdConstexpr("temple_block_0"), 0, 0}, 1},

    {Tile::BossAir0, {HashAFrameIdConstexpr("boss_air_0"), 0, 0}, 1},
    {Tile::BossAir1, {HashAFrameIdConstexpr("boss_air_1"), 0, 0}, 1},
    {Tile::BossAir2, {HashAFrameIdConstexpr("boss_air_2"), 0, 0}, 1},
    {Tile::BossDirt,
     {HashAFrameIdConstexpr("boss_dirt_0"), HashAFrameIdConstexpr("boss_dirt_1"),
      HashAFrameIdConstexpr("boss_dirt_2")},
     3},
    {Tile::BossBlock, {HashAFrameIdConstexpr("boss_block_0"), 0, 0}, 1},

    {Tile::LadderTop, {HashAFrameIdConstexpr("ladder_top_0"), 0, 0}, 1},
    {Tile::Ladder, {HashAFrameIdConstexpr("ladder_0"), 0, 0}, 1},
    {Tile::LadderOrange, {HashAFrameIdConstexpr("ladder_0"), 0, 0}, 1},
    {Tile::Spikes, {HashAFrameIdConstexpr("spikes_0"), 0, 0}, 1},
    {Tile::Rope, {HashAFrameIdConstexpr("rope"), 0, 0}, 1},
    {Tile::Vine, {HashAFrameIdConstexpr("rope"), 0, 0}, 1},
    {Tile::VineTop, {HashAFrameIdConstexpr("rope"), 0, 0}, 1},
    {Tile::WaterSwim, {HashAFrameIdConstexpr("water"), 0, 0}, 1},
    {Tile::WaterTop, {HashAFrameIdConstexpr("watertop"), 0, 0}, 1},
    {Tile::Lava, {HashAFrameIdConstexpr("cave_air_0"), 0, 0}, 1},
    {Tile::Lush,
     {HashAFrameIdConstexpr("jungle_dirt_0"), HashAFrameIdConstexpr("jungle_dirt_1"),
      HashAFrameIdConstexpr("jungle_dirt_2")},
     3},
    {Tile::Tree, {HashAFrameIdConstexpr("jungle_block_0"), 0, 0}, 1},
    {Tile::ThinIce, {HashAFrameIdConstexpr("ice_block_0"), 0, 0}, 1},
    {Tile::Dark,
     {HashAFrameIdConstexpr("ice_dirt_0"), HashAFrameIdConstexpr("ice_dirt_1"),
      HashAFrameIdConstexpr("ice_dirt_2")},
     3},
    {Tile::DarkFall, {HashAFrameIdConstexpr("ice_block_0"), 0, 0}, 1},
    {Tile::AlienShip, {HashAFrameIdConstexpr("ice_block_0"), 0, 0}, 1},
    {Tile::TempleFake,
     {HashAFrameIdConstexpr("temple_dirt_0"), HashAFrameIdConstexpr("temple_dirt_1"),
      HashAFrameIdConstexpr("temple_dirt_2")},
     3},
    {Tile::Entrance, {HashAFrameIdConstexpr("entrance"), 0, 0}, 1},
    {Tile::Exit, {HashAFrameIdConstexpr("exit"), 0, 0}, 1},
}};

const AFrame&
RequireSingleFrame(const AFrameDb& aframe_db,
                   const std::unordered_map<AFrameId, std::size_t>& anim_index_by_id,
                   std::uint32_t name_hash) {
    const auto found = anim_index_by_id.find(name_hash);
    if (found == anim_index_by_id.end()) {
        throw std::runtime_error("TileSourceData build error: mapped frame name hash not found");
    }

    const AFrameAnim& anim = aframe_db.anims[found->second];
    if (anim.frame_indices.empty()) {
        throw std::runtime_error("TileSourceData build error: mapped anim has no frames");
    }
    return aframe_db.frames[anim.frame_indices.front()];
}

const TileSourceSpan* FindTileSourceSpan(const TileSourceDb& tile_source_db, Tile tile) {
    const std::size_t slot_index = TileToIndex(tile);
    if (slot_index >= tile_source_db.tile_spans.size()) {
        return nullptr;
    }
    const TileSourceSpan& span = tile_source_db.tile_spans[slot_index];
    if (span.source_count == 0) {
        return nullptr;
    }
    return &span;
}

const TileSourceData* GetSourceDataForSpan(Graphics& graphics, const TileSourceSpan* span,
                                           const IVec2& tile_pos) {
    if (span == nullptr || span->first_source_index >= graphics.tile_source_db.sources.size()) {
        return nullptr;
    }

    std::uint32_t variation = 0;
    if (span->source_count > 1) {
        const std::uint64_t key = TileVariationCacheKey(tile_pos);
        const auto found = graphics.tile_variations_cache.find(key);
        if (found != graphics.tile_variations_cache.end()) {
            variation = found->second % span->source_count;
        } else {
            const std::uint32_t seed =
                static_cast<std::uint32_t>(static_cast<std::uint32_t>(tile_pos.x) * 73856093U) ^
                static_cast<std::uint32_t>(static_cast<std::uint32_t>(tile_pos.y) * 19349663U);
            variation = seed % span->source_count;
            graphics.tile_variations_cache.insert({key, variation});
        }
    }

    const std::size_t source_index = static_cast<std::size_t>(span->first_source_index + variation);
    if (source_index >= graphics.tile_source_db.sources.size()) {
        return nullptr;
    }
    return &graphics.tile_source_db.sources[source_index];
}

} // namespace

TileSourceDb BuildTileSourceDb(const AFrameDb& aframe_db) {
    TileSourceDb tile_source_db;
    tile_source_db.tile_spans.resize(kTileCount);

    for (const TileSourceNameGroup& group : kTileSourceNameGroups) {
        TileSourceSpan& span = tile_source_db.tile_spans[TileToIndex(group.tile)];
        span.first_source_index = static_cast<std::uint32_t>(tile_source_db.sources.size());
        span.source_count = group.count;

        for (std::uint32_t i = 0; i < group.count; ++i) {
            const AFrame& aframe = RequireSingleFrame(
                aframe_db, aframe_db.anim_indices_by_id, group.name_hashes[i]);
            tile_source_db.sources.push_back(TileSourceData{
                .image_id = aframe.image_id,
                .sample_rect = aframe.sample_rect,
                .cbox = aframe.cbox,
            });
        }
    }

    return tile_source_db;
}

const TileSourceData* GetTileSourceData(Graphics& graphics, Tile tile, const IVec2& tile_pos) {
    if (tile == Tile::Air) {
        return nullptr;
    }
    return GetSourceDataForSpan(graphics, FindTileSourceSpan(graphics.tile_source_db, tile),
                                tile_pos);
}

const TileSourceData* GetTileSourceDataForStage(
    Graphics& graphics,
    const Stage& stage,
    Tile tile,
    const IVec2& tile_pos
) {
    if (tile == Tile::WaterSwim && IsWaterSurfaceTile(stage, tile_pos)) {
        return GetTileSourceData(graphics, Tile::WaterTop, tile_pos);
    }
    return GetTileSourceData(graphics, tile, tile_pos);
}

SDL_Texture* GetTileTexture(const Graphics& graphics, const TileSourceData& tile_source_data) {
    const std::size_t image_index = static_cast<std::size_t>(tile_source_data.image_id);
    if (image_index >= graphics.aframe_images.size()) {
        return nullptr;
    }
    return graphics.aframe_images[image_index];
}

} // namespace splonks

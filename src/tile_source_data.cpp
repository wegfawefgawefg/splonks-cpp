#include "tile_source_data.hpp"

#include "graphics.hpp"

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

constexpr std::array<TileSourceNameGroup, 62> kTileSourceNameGroups{{
    {Tile::CaveAir0, {HashFrameDataIdConstexpr("cave_air_0"), 0, 0}, 1},
    {Tile::CaveAir1, {HashFrameDataIdConstexpr("cave_air_1"), 0, 0}, 1},
    {Tile::CaveAir2, {HashFrameDataIdConstexpr("cave_air_2"), 0, 0}, 1},
    {Tile::CaveDirt,
     {HashFrameDataIdConstexpr("cave_dirt_0"), HashFrameDataIdConstexpr("cave_dirt_1"),
      HashFrameDataIdConstexpr("cave_dirt_2")},
     3},
    {Tile::CaveGold, {HashFrameDataIdConstexpr("cave_gold_0"), 0, 0}, 1},
    {Tile::CaveGoldBig, {HashFrameDataIdConstexpr("cave_gold_1"), 0, 0}, 1},
    {Tile::CaveBlock, {HashFrameDataIdConstexpr("cave_block_0"), 0, 0}, 1},
    {Tile::CaveShopWall, {HashFrameDataIdConstexpr("cave_shop_wall"), 0, 0}, 1},
    {Tile::CaveSmoothWall, {HashFrameDataIdConstexpr("cave_smooth_wall"), 0, 0}, 1},
    {Tile::Glass, {HashFrameDataIdConstexpr("glass"), 0, 0}, 1},
    {Tile::LawsonWall, {HashFrameDataIdConstexpr("lawson_wall"), 0, 0}, 1},
    {Tile::LawsonInside, {HashFrameDataIdConstexpr("lawson_inside"), 0, 0}, 1},
    {Tile::LawsonLeftTopper, {HashFrameDataIdConstexpr("lawson_left_topper"), 0, 0}, 1},
    {Tile::LawsonMiddleTopper, {HashFrameDataIdConstexpr("lawson_middle_topper"), 0, 0}, 1},
    {Tile::LawsonRightTopper, {HashFrameDataIdConstexpr("lawson_right_topper"), 0, 0}, 1},
    {Tile::LawsonFloor, {HashFrameDataIdConstexpr("lawson_floor"), 0, 0}, 1},

    {Tile::IceAir0, {HashFrameDataIdConstexpr("ice_air_0"), 0, 0}, 1},
    {Tile::IceAir1, {HashFrameDataIdConstexpr("ice_air_1"), 0, 0}, 1},
    {Tile::IceAir2, {HashFrameDataIdConstexpr("ice_air_2"), 0, 0}, 1},
    {Tile::IceDirt,
     {HashFrameDataIdConstexpr("ice_dirt_0"), HashFrameDataIdConstexpr("ice_dirt_1"),
      HashFrameDataIdConstexpr("ice_dirt_2")},
     3},
    {Tile::IceGold, {HashFrameDataIdConstexpr("ice_gold"), 0, 0}, 1},
    {Tile::IceGoldBig, {HashFrameDataIdConstexpr("ice_gold"), 0, 0}, 1},
    {Tile::IceBlock, {HashFrameDataIdConstexpr("ice_block_0"), 0, 0}, 1},

    {Tile::JungleAir0, {HashFrameDataIdConstexpr("jungle_air_0"), 0, 0}, 1},
    {Tile::JungleAir1, {HashFrameDataIdConstexpr("jungle_air_1"), 0, 0}, 1},
    {Tile::JungleAir2, {HashFrameDataIdConstexpr("jungle_air_2"), 0, 0}, 1},
    {Tile::JungleDirt,
     {HashFrameDataIdConstexpr("jungle_dirt_0"), HashFrameDataIdConstexpr("jungle_dirt_1"),
      HashFrameDataIdConstexpr("jungle_dirt_2")},
     3},
    {Tile::JungleGold, {HashFrameDataIdConstexpr("jungle_gold_0"), 0, 0}, 1},
    {Tile::JungleGoldBig, {HashFrameDataIdConstexpr("jungle_gold_0"), 0, 0}, 1},
    {Tile::JungleBlock, {HashFrameDataIdConstexpr("jungle_block_0"), 0, 0}, 1},

    {Tile::TempleAir0, {HashFrameDataIdConstexpr("temple_air_0"), 0, 0}, 1},
    {Tile::TempleAir1, {HashFrameDataIdConstexpr("temple_air_1"), 0, 0}, 1},
    {Tile::TempleAir2, {HashFrameDataIdConstexpr("temple_air_2"), 0, 0}, 1},
    {Tile::TempleDirt,
     {HashFrameDataIdConstexpr("temple_dirt_0"), HashFrameDataIdConstexpr("temple_dirt_1"),
      HashFrameDataIdConstexpr("temple_dirt_2")},
     3},
    {Tile::TempleGold, {HashFrameDataIdConstexpr("temple_gold"), 0, 0}, 1},
    {Tile::TempleGoldBig, {HashFrameDataIdConstexpr("temple_gold"), 0, 0}, 1},
    {Tile::TempleBlock, {HashFrameDataIdConstexpr("temple_block_0"), 0, 0}, 1},

    {Tile::BossAir0, {HashFrameDataIdConstexpr("boss_air_0"), 0, 0}, 1},
    {Tile::BossAir1, {HashFrameDataIdConstexpr("boss_air_1"), 0, 0}, 1},
    {Tile::BossAir2, {HashFrameDataIdConstexpr("boss_air_2"), 0, 0}, 1},
    {Tile::BossDirt,
     {HashFrameDataIdConstexpr("boss_dirt_0"), HashFrameDataIdConstexpr("boss_dirt_1"),
      HashFrameDataIdConstexpr("boss_dirt_2")},
     3},
    {Tile::BossGold, {HashFrameDataIdConstexpr("boss_gold"), 0, 0}, 1},
    {Tile::BossGoldBig, {HashFrameDataIdConstexpr("boss_gold"), 0, 0}, 1},
    {Tile::BossBlock, {HashFrameDataIdConstexpr("boss_block_0"), 0, 0}, 1},

    {Tile::LadderTop, {HashFrameDataIdConstexpr("ladder_top_0"), 0, 0}, 1},
    {Tile::Ladder, {HashFrameDataIdConstexpr("ladder_0"), 0, 0}, 1},
    {Tile::LadderOrange, {HashFrameDataIdConstexpr("ladder_0"), 0, 0}, 1},
    {Tile::Spikes, {HashFrameDataIdConstexpr("spikes_0"), 0, 0}, 1},
    {Tile::Rope, {HashFrameDataIdConstexpr("rope"), 0, 0}, 1},
    {Tile::Vine, {HashFrameDataIdConstexpr("rope"), 0, 0}, 1},
    {Tile::VineTop, {HashFrameDataIdConstexpr("rope"), 0, 0}, 1},
    {Tile::WaterSwim, {HashFrameDataIdConstexpr("cave_air_0"), 0, 0}, 1},
    {Tile::Lava, {HashFrameDataIdConstexpr("cave_air_0"), 0, 0}, 1},
    {Tile::Lush,
     {HashFrameDataIdConstexpr("jungle_dirt_0"), HashFrameDataIdConstexpr("jungle_dirt_1"),
      HashFrameDataIdConstexpr("jungle_dirt_2")},
     3},
    {Tile::Tree, {HashFrameDataIdConstexpr("jungle_block_0"), 0, 0}, 1},
    {Tile::ThinIce, {HashFrameDataIdConstexpr("ice_block_0"), 0, 0}, 1},
    {Tile::Dark,
     {HashFrameDataIdConstexpr("ice_dirt_0"), HashFrameDataIdConstexpr("ice_dirt_1"),
      HashFrameDataIdConstexpr("ice_dirt_2")},
     3},
    {Tile::DarkFall, {HashFrameDataIdConstexpr("ice_block_0"), 0, 0}, 1},
    {Tile::AlienShip, {HashFrameDataIdConstexpr("ice_block_0"), 0, 0}, 1},
    {Tile::TempleFake,
     {HashFrameDataIdConstexpr("temple_dirt_0"), HashFrameDataIdConstexpr("temple_dirt_1"),
      HashFrameDataIdConstexpr("temple_dirt_2")},
     3},
    {Tile::Entrance, {HashFrameDataIdConstexpr("entrance"), 0, 0}, 1},
    {Tile::Exit, {HashFrameDataIdConstexpr("exit"), 0, 0}, 1},
}};

const FrameData&
RequireSingleFrame(const FrameDataDb& frame_data_db,
                   const std::unordered_map<FrameDataId, std::size_t>& animation_index_by_id,
                   std::uint32_t name_hash) {
    const auto found = animation_index_by_id.find(name_hash);
    if (found == animation_index_by_id.end()) {
        throw std::runtime_error("TileSourceData build error: mapped frame name hash not found");
    }

    const FrameDataAnimation& animation = frame_data_db.animations[found->second];
    if (animation.frame_indices.empty()) {
        throw std::runtime_error("TileSourceData build error: mapped animation has no frames");
    }
    return frame_data_db.frames[animation.frame_indices.front()];
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

TileSourceDb BuildTileSourceDb(const FrameDataDb& frame_data_db) {
    TileSourceDb tile_source_db;
    tile_source_db.tile_spans.resize(kTileCount);

    for (const TileSourceNameGroup& group : kTileSourceNameGroups) {
        TileSourceSpan& span = tile_source_db.tile_spans[TileToIndex(group.tile)];
        span.first_source_index = static_cast<std::uint32_t>(tile_source_db.sources.size());
        span.source_count = group.count;

        for (std::uint32_t i = 0; i < group.count; ++i) {
            const FrameData& frame_data = RequireSingleFrame(
                frame_data_db, frame_data_db.animation_indices_by_id, group.name_hashes[i]);
            tile_source_db.sources.push_back(TileSourceData{
                .image_id = frame_data.image_id,
                .sample_rect = frame_data.sample_rect,
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

SDL_Texture* GetTileTexture(const Graphics& graphics, const TileSourceData& tile_source_data) {
    const std::size_t image_index = static_cast<std::size_t>(tile_source_data.image_id);
    if (image_index >= graphics.frame_data_images.size()) {
        return nullptr;
    }
    return graphics.frame_data_images[image_index];
}

} // namespace splonks

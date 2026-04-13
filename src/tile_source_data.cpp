#include "tile_source_data.hpp"

#include "graphics.hpp"

#include <array>
#include <stdexcept>

namespace splonks {

namespace {

constexpr std::size_t TileToIndex(Tile tile) {
    return static_cast<std::size_t>(tile);
}

constexpr std::size_t TileSetToIndex(TileSet tile_set) {
    return static_cast<std::size_t>(tile_set);
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

struct AirSourceNameGroup {
    TileSet tile_set;
    std::array<std::uint32_t, 3> name_hashes{};
    std::uint32_t count = 0;
};

constexpr std::array<TileSourceNameGroup, 29> kTileSourceNameGroups{{
    {Tile::CaveDirt, {HashFrameDataIdConstexpr("cave_dirt_0"), HashFrameDataIdConstexpr("cave_dirt_1"), HashFrameDataIdConstexpr("cave_dirt_2")}, 3},
    {Tile::CaveGold, {HashFrameDataIdConstexpr("cave_gold_0"), 0, 0}, 1},
    {Tile::CaveGoldBig, {HashFrameDataIdConstexpr("cave_gold_1"), 0, 0}, 1},
    {Tile::CaveBlock, {HashFrameDataIdConstexpr("cave_block_0"), 0, 0}, 1},
    {Tile::CaveShopWall, {HashFrameDataIdConstexpr("cave_shop_wall"), 0, 0}, 1},
    {Tile::CaveSmoothWall, {HashFrameDataIdConstexpr("cave_smooth_wall"), 0, 0}, 1},
    {Tile::Glass, {HashFrameDataIdConstexpr("glass"), 0, 0}, 1},

    {Tile::IceDirt, {HashFrameDataIdConstexpr("ice_dirt_0"), HashFrameDataIdConstexpr("ice_dirt_1"), HashFrameDataIdConstexpr("ice_dirt_2")}, 3},
    {Tile::IceGold, {HashFrameDataIdConstexpr("ice_gold"), 0, 0}, 1},
    {Tile::IceGoldBig, {HashFrameDataIdConstexpr("ice_gold"), 0, 0}, 1},
    {Tile::IceBlock, {HashFrameDataIdConstexpr("ice_block_0"), 0, 0}, 1},

    {Tile::JungleDirt, {HashFrameDataIdConstexpr("jungle_dirt_0"), HashFrameDataIdConstexpr("jungle_dirt_1"), HashFrameDataIdConstexpr("jungle_dirt_2")}, 3},
    {Tile::JungleGold, {HashFrameDataIdConstexpr("jungle_gold_0"), 0, 0}, 1},
    {Tile::JungleGoldBig, {HashFrameDataIdConstexpr("jungle_gold_0"), 0, 0}, 1},
    {Tile::JungleBlock, {HashFrameDataIdConstexpr("jungle_block_0"), 0, 0}, 1},

    {Tile::TempleDirt, {HashFrameDataIdConstexpr("temple_dirt_0"), HashFrameDataIdConstexpr("temple_dirt_1"), HashFrameDataIdConstexpr("temple_dirt_2")}, 3},
    {Tile::TempleGold, {HashFrameDataIdConstexpr("temple_gold"), 0, 0}, 1},
    {Tile::TempleGoldBig, {HashFrameDataIdConstexpr("temple_gold"), 0, 0}, 1},
    {Tile::TempleBlock, {HashFrameDataIdConstexpr("temple_block_0"), 0, 0}, 1},

    {Tile::BossDirt, {HashFrameDataIdConstexpr("boss_dirt_0"), HashFrameDataIdConstexpr("boss_dirt_1"), HashFrameDataIdConstexpr("boss_dirt_2")}, 3},
    {Tile::BossGold, {HashFrameDataIdConstexpr("boss_gold"), 0, 0}, 1},
    {Tile::BossGoldBig, {HashFrameDataIdConstexpr("boss_gold"), 0, 0}, 1},
    {Tile::BossBlock, {HashFrameDataIdConstexpr("boss_block_0"), 0, 0}, 1},

    {Tile::LadderTop, {HashFrameDataIdConstexpr("ladder_top_0"), 0, 0}, 1},
    {Tile::Ladder, {HashFrameDataIdConstexpr("ladder_0"), 0, 0}, 1},
    {Tile::Spikes, {HashFrameDataIdConstexpr("spikes_0"), 0, 0}, 1},
    {Tile::Rope, {HashFrameDataIdConstexpr("rope"), 0, 0}, 1},
    {Tile::Entrance, {HashFrameDataIdConstexpr("entrance"), 0, 0}, 1},
    {Tile::Exit, {HashFrameDataIdConstexpr("exit"), 0, 0}, 1},
}};

constexpr std::array<AirSourceNameGroup, 5> kAirSourceNameGroups{{
    {TileSet::Cave, {HashFrameDataIdConstexpr("cave_air_0"), HashFrameDataIdConstexpr("cave_air_1"), HashFrameDataIdConstexpr("cave_air_2")}, 3},
    {TileSet::Ice, {HashFrameDataIdConstexpr("ice_air_0"), HashFrameDataIdConstexpr("ice_air_1"), HashFrameDataIdConstexpr("ice_air_2")}, 3},
    {TileSet::Jungle, {HashFrameDataIdConstexpr("jungle_air_0"), HashFrameDataIdConstexpr("jungle_air_1"), HashFrameDataIdConstexpr("jungle_air_2")}, 3},
    {TileSet::Temple, {HashFrameDataIdConstexpr("temple_air_0"), HashFrameDataIdConstexpr("temple_air_1"), HashFrameDataIdConstexpr("temple_air_2")}, 3},
    {TileSet::Boss, {HashFrameDataIdConstexpr("boss_air_0"), HashFrameDataIdConstexpr("boss_air_1"), HashFrameDataIdConstexpr("boss_air_2")}, 3},
}};

const FrameData& RequireSingleFrame(
    const FrameDataDb& frame_data_db,
    const std::unordered_map<FrameDataId, std::size_t>& animation_index_by_id,
    std::uint32_t name_hash
) {
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

const TileSourceSpan* FindAirSourceSpan(const TileSourceDb& tile_source_db, TileSet tile_set) {
    const std::size_t slot_index = TileSetToIndex(tile_set);
    if (slot_index >= tile_source_db.air_spans.size()) {
        return nullptr;
    }
    const TileSourceSpan& span = tile_source_db.air_spans[slot_index];
    if (span.source_count == 0) {
        return nullptr;
    }
    return &span;
}

const TileSourceData* GetSourceDataForSpan(
    Graphics& graphics,
    const TileSourceSpan* span,
    const IVec2& tile_pos
) {
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

    const std::size_t source_index =
        static_cast<std::size_t>(span->first_source_index + variation);
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
            const FrameData& frame_data =
                RequireSingleFrame(frame_data_db, frame_data_db.animation_indices_by_id, group.name_hashes[i]);
            tile_source_db.sources.push_back(TileSourceData{
                .image_id = frame_data.image_id,
                .sample_rect = frame_data.sample_rect,
            });
        }
    }

    for (const AirSourceNameGroup& group : kAirSourceNameGroups) {
        TileSourceSpan& span = tile_source_db.air_spans[TileSetToIndex(group.tile_set)];
        span.first_source_index = static_cast<std::uint32_t>(tile_source_db.sources.size());
        span.source_count = group.count;

        for (std::uint32_t i = 0; i < group.count; ++i) {
            const FrameData& frame_data =
                RequireSingleFrame(frame_data_db, frame_data_db.animation_indices_by_id, group.name_hashes[i]);
            tile_source_db.sources.push_back(TileSourceData{
                .image_id = frame_data.image_id,
                .sample_rect = frame_data.sample_rect,
            });
        }
    }

    return tile_source_db;
}

TileSet TileSetForStageType(StageType stage_type) {
    switch (stage_type) {
    case StageType::Ice1:
    case StageType::Ice2:
    case StageType::Ice3:
        return TileSet::Ice;
    case StageType::Desert1:
    case StageType::Desert2:
    case StageType::Desert3:
        return TileSet::Jungle;
    case StageType::Temple1:
    case StageType::Temple2:
    case StageType::Temple3:
        return TileSet::Temple;
    case StageType::Boss:
        return TileSet::Boss;
    case StageType::Blank:
    case StageType::Test1:
    case StageType::Cave1:
    case StageType::Cave2:
    case StageType::Cave3:
        return TileSet::Cave;
    }

    return TileSet::Cave;
}

const TileSourceData* GetTileSourceData(Graphics& graphics, Tile tile, const IVec2& tile_pos) {
    if (tile == Tile::Air) {
        return nullptr;
    }
    return GetSourceDataForSpan(graphics, FindTileSourceSpan(graphics.tile_source_db, tile), tile_pos);
}

const TileSourceData* GetAirSourceData(Graphics& graphics, TileSet tile_set, const IVec2& tile_pos) {
    return GetSourceDataForSpan(graphics, FindAirSourceSpan(graphics.tile_source_db, tile_set), tile_pos);
}

SDL_Texture* GetTileTexture(const Graphics& graphics, const TileSourceData& tile_source_data) {
    const std::size_t image_index = static_cast<std::size_t>(tile_source_data.image_id);
    if (image_index >= graphics.frame_data_images.size()) {
        return nullptr;
    }
    return graphics.frame_data_images[image_index];
}

} // namespace splonks

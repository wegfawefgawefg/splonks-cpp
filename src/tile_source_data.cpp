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

constexpr std::size_t TileSourceSlotIndex(TileSet tile_set, Tile tile) {
    return (TileSetToIndex(tile_set) * kTileCount) + TileToIndex(tile);
}

std::uint64_t TileVariationCacheKey(const IVec2& tile_pos) {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(tile_pos.x)) << 32U) |
           static_cast<std::uint32_t>(tile_pos.y);
}

struct TileSourceNameGroup {
    TileSet tile_set;
    Tile tile;
    std::array<std::uint32_t, 3> name_hashes{};
    std::uint32_t count = 0;
};

constexpr std::array<TileSourceNameGroup, 50> kTileSourceNameGroups{{
    {TileSet::Cave, Tile::Air, {HashFrameDataIdConstexpr("cave_air_0"), HashFrameDataIdConstexpr("cave_air_1"), HashFrameDataIdConstexpr("cave_air_2")}, 3},
    {TileSet::Cave, Tile::Dirt, {HashFrameDataIdConstexpr("cave_dirt_0"), HashFrameDataIdConstexpr("cave_dirt_1"), HashFrameDataIdConstexpr("cave_dirt_2")}, 3},
    {TileSet::Cave, Tile::Gold, {HashFrameDataIdConstexpr("cave_gold_0"), 0, 0}, 1},
    {TileSet::Cave, Tile::Block, {HashFrameDataIdConstexpr("cave_block_0"), 0, 0}, 1},
    {TileSet::Cave, Tile::LadderTop, {HashFrameDataIdConstexpr("ladder_top_0"), 0, 0}, 1},
    {TileSet::Cave, Tile::Ladder, {HashFrameDataIdConstexpr("ladder_0"), 0, 0}, 1},
    {TileSet::Cave, Tile::Spikes, {HashFrameDataIdConstexpr("spikes_0"), 0, 0}, 1},
    {TileSet::Cave, Tile::Rope, {HashFrameDataIdConstexpr("rope"), 0, 0}, 1},
    {TileSet::Cave, Tile::Entrance, {HashFrameDataIdConstexpr("entrance"), 0, 0}, 1},
    {TileSet::Cave, Tile::Exit, {HashFrameDataIdConstexpr("exit"), 0, 0}, 1},

    {TileSet::Ice, Tile::Air, {HashFrameDataIdConstexpr("ice_air_0"), HashFrameDataIdConstexpr("ice_air_1"), HashFrameDataIdConstexpr("ice_air_2")}, 3},
    {TileSet::Ice, Tile::Dirt, {HashFrameDataIdConstexpr("ice_dirt_0"), HashFrameDataIdConstexpr("ice_dirt_1"), HashFrameDataIdConstexpr("ice_dirt_2")}, 3},
    {TileSet::Ice, Tile::Gold, {HashFrameDataIdConstexpr("ice_gold"), 0, 0}, 1},
    {TileSet::Ice, Tile::Block, {HashFrameDataIdConstexpr("ice_block_0"), 0, 0}, 1},
    {TileSet::Ice, Tile::LadderTop, {HashFrameDataIdConstexpr("ladder_top_0"), 0, 0}, 1},
    {TileSet::Ice, Tile::Ladder, {HashFrameDataIdConstexpr("ladder_0"), 0, 0}, 1},
    {TileSet::Ice, Tile::Spikes, {HashFrameDataIdConstexpr("spikes_0"), 0, 0}, 1},
    {TileSet::Ice, Tile::Rope, {HashFrameDataIdConstexpr("rope"), 0, 0}, 1},
    {TileSet::Ice, Tile::Entrance, {HashFrameDataIdConstexpr("entrance"), 0, 0}, 1},
    {TileSet::Ice, Tile::Exit, {HashFrameDataIdConstexpr("exit"), 0, 0}, 1},

    {TileSet::Jungle, Tile::Air, {HashFrameDataIdConstexpr("jungle_air_0"), HashFrameDataIdConstexpr("jungle_air_1"), HashFrameDataIdConstexpr("jungle_air_2")}, 3},
    {TileSet::Jungle, Tile::Dirt, {HashFrameDataIdConstexpr("jungle_dirt_0"), HashFrameDataIdConstexpr("jungle_dirt_1"), HashFrameDataIdConstexpr("jungle_dirt_2")}, 3},
    {TileSet::Jungle, Tile::Gold, {HashFrameDataIdConstexpr("jungle_gold_0"), 0, 0}, 1},
    {TileSet::Jungle, Tile::Block, {HashFrameDataIdConstexpr("jungle_block_0"), 0, 0}, 1},
    {TileSet::Jungle, Tile::LadderTop, {HashFrameDataIdConstexpr("ladder_top_0"), 0, 0}, 1},
    {TileSet::Jungle, Tile::Ladder, {HashFrameDataIdConstexpr("ladder_0"), 0, 0}, 1},
    {TileSet::Jungle, Tile::Spikes, {HashFrameDataIdConstexpr("spikes_0"), 0, 0}, 1},
    {TileSet::Jungle, Tile::Rope, {HashFrameDataIdConstexpr("rope"), 0, 0}, 1},
    {TileSet::Jungle, Tile::Entrance, {HashFrameDataIdConstexpr("entrance"), 0, 0}, 1},
    {TileSet::Jungle, Tile::Exit, {HashFrameDataIdConstexpr("exit"), 0, 0}, 1},

    {TileSet::Temple, Tile::Air, {HashFrameDataIdConstexpr("temple_air_0"), HashFrameDataIdConstexpr("temple_air_1"), HashFrameDataIdConstexpr("temple_air_2")}, 3},
    {TileSet::Temple, Tile::Dirt, {HashFrameDataIdConstexpr("temple_dirt_0"), HashFrameDataIdConstexpr("temple_dirt_1"), HashFrameDataIdConstexpr("temple_dirt_2")}, 3},
    {TileSet::Temple, Tile::Gold, {HashFrameDataIdConstexpr("temple_gold"), 0, 0}, 1},
    {TileSet::Temple, Tile::Block, {HashFrameDataIdConstexpr("temple_block_0"), 0, 0}, 1},
    {TileSet::Temple, Tile::LadderTop, {HashFrameDataIdConstexpr("ladder_top_0"), 0, 0}, 1},
    {TileSet::Temple, Tile::Ladder, {HashFrameDataIdConstexpr("ladder_0"), 0, 0}, 1},
    {TileSet::Temple, Tile::Spikes, {HashFrameDataIdConstexpr("spikes_0"), 0, 0}, 1},
    {TileSet::Temple, Tile::Rope, {HashFrameDataIdConstexpr("rope"), 0, 0}, 1},
    {TileSet::Temple, Tile::Entrance, {HashFrameDataIdConstexpr("entrance"), 0, 0}, 1},
    {TileSet::Temple, Tile::Exit, {HashFrameDataIdConstexpr("exit"), 0, 0}, 1},

    {TileSet::Boss, Tile::Air, {HashFrameDataIdConstexpr("boss_air_0"), HashFrameDataIdConstexpr("boss_air_1"), HashFrameDataIdConstexpr("boss_air_2")}, 3},
    {TileSet::Boss, Tile::Dirt, {HashFrameDataIdConstexpr("boss_dirt_0"), HashFrameDataIdConstexpr("boss_dirt_1"), HashFrameDataIdConstexpr("boss_dirt_2")}, 3},
    {TileSet::Boss, Tile::Gold, {HashFrameDataIdConstexpr("boss_gold"), 0, 0}, 1},
    {TileSet::Boss, Tile::Block, {HashFrameDataIdConstexpr("boss_block_0"), 0, 0}, 1},
    {TileSet::Boss, Tile::LadderTop, {HashFrameDataIdConstexpr("ladder_top_0"), 0, 0}, 1},
    {TileSet::Boss, Tile::Ladder, {HashFrameDataIdConstexpr("ladder_0"), 0, 0}, 1},
    {TileSet::Boss, Tile::Spikes, {HashFrameDataIdConstexpr("spikes_0"), 0, 0}, 1},
    {TileSet::Boss, Tile::Rope, {HashFrameDataIdConstexpr("rope"), 0, 0}, 1},
    {TileSet::Boss, Tile::Entrance, {HashFrameDataIdConstexpr("entrance"), 0, 0}, 1},
    {TileSet::Boss, Tile::Exit, {HashFrameDataIdConstexpr("exit"), 0, 0}, 1},
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

const TileSourceSpan* FindTileSourceSpan(const TileSourceDb& tile_source_db, TileSet tile_set, Tile tile) {
    const std::size_t slot_index = TileSourceSlotIndex(tile_set, tile);
    if (slot_index >= tile_source_db.spans.size()) {
        return nullptr;
    }
    const TileSourceSpan& span = tile_source_db.spans[slot_index];
    if (span.source_count == 0) {
        return nullptr;
    }
    return &span;
}

} // namespace

TileSourceDb BuildTileSourceDb(const FrameDataDb& frame_data_db) {
    TileSourceDb tile_source_db;
    tile_source_db.spans.resize(kTileSetCount * kTileCount);

    for (const TileSourceNameGroup& group : kTileSourceNameGroups) {
        TileSourceSpan& span =
            tile_source_db.spans[TileSourceSlotIndex(group.tile_set, group.tile)];
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

const TileSourceData* GetTileSourceData(
    Graphics& graphics,
    TileSet tile_set,
    Tile tile,
    const IVec2& tile_pos
) {
    const TileSourceSpan* const span = FindTileSourceSpan(graphics.tile_source_db, tile_set, tile);
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

SDL_Texture* GetTileTexture(const Graphics& graphics, const TileSourceData& tile_source_data) {
    const std::size_t image_index = static_cast<std::size_t>(tile_source_data.image_id);
    if (image_index >= graphics.frame_data_images.size()) {
        return nullptr;
    }
    return graphics.frame_data_images[image_index];
}

} // namespace splonks

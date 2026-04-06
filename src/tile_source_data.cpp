#include "tile_source_data.hpp"

#include "graphics.hpp"

#include <array>
#include <stdexcept>
#include <string_view>
#include <unordered_map>

namespace splonks {

namespace {

constexpr std::uint32_t FnvOffsetBasis32 = 2166136261U;
constexpr std::uint32_t FnvPrime32 = 16777619U;

constexpr std::uint32_t HashFrameDataNameConstexpr(std::string_view text) {
    std::uint32_t hash = FnvOffsetBasis32;
    for (char character : text) {
        hash ^= static_cast<std::uint32_t>(static_cast<unsigned char>(character));
        hash *= FnvPrime32;
    }
    return hash;
}

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
    {TileSet::Cave, Tile::Air, {HashFrameDataNameConstexpr("cave_air_0"), HashFrameDataNameConstexpr("cave_air_1"), HashFrameDataNameConstexpr("cave_air_2")}, 3},
    {TileSet::Cave, Tile::Dirt, {HashFrameDataNameConstexpr("cave_dirt_0"), HashFrameDataNameConstexpr("cave_dirt_1"), HashFrameDataNameConstexpr("cave_dirt_2")}, 3},
    {TileSet::Cave, Tile::Gold, {HashFrameDataNameConstexpr("cave_gold_0"), 0, 0}, 1},
    {TileSet::Cave, Tile::Block, {HashFrameDataNameConstexpr("cave_block_0"), 0, 0}, 1},
    {TileSet::Cave, Tile::LadderTop, {HashFrameDataNameConstexpr("ladder_top_0"), 0, 0}, 1},
    {TileSet::Cave, Tile::Ladder, {HashFrameDataNameConstexpr("ladder_0"), 0, 0}, 1},
    {TileSet::Cave, Tile::Spikes, {HashFrameDataNameConstexpr("spikes_0"), 0, 0}, 1},
    {TileSet::Cave, Tile::Rope, {HashFrameDataNameConstexpr("rope"), 0, 0}, 1},
    {TileSet::Cave, Tile::Entrance, {HashFrameDataNameConstexpr("entrance"), 0, 0}, 1},
    {TileSet::Cave, Tile::Exit, {HashFrameDataNameConstexpr("exit"), 0, 0}, 1},

    {TileSet::Ice, Tile::Air, {HashFrameDataNameConstexpr("ice_air_0"), HashFrameDataNameConstexpr("ice_air_1"), HashFrameDataNameConstexpr("ice_air_2")}, 3},
    {TileSet::Ice, Tile::Dirt, {HashFrameDataNameConstexpr("ice_dirt_0"), HashFrameDataNameConstexpr("ice_dirt_1"), HashFrameDataNameConstexpr("ice_dirt_2")}, 3},
    {TileSet::Ice, Tile::Gold, {HashFrameDataNameConstexpr("ice_gold"), 0, 0}, 1},
    {TileSet::Ice, Tile::Block, {HashFrameDataNameConstexpr("ice_block_0"), 0, 0}, 1},
    {TileSet::Ice, Tile::LadderTop, {HashFrameDataNameConstexpr("ladder_top_0"), 0, 0}, 1},
    {TileSet::Ice, Tile::Ladder, {HashFrameDataNameConstexpr("ladder_0"), 0, 0}, 1},
    {TileSet::Ice, Tile::Spikes, {HashFrameDataNameConstexpr("spikes_0"), 0, 0}, 1},
    {TileSet::Ice, Tile::Rope, {HashFrameDataNameConstexpr("rope"), 0, 0}, 1},
    {TileSet::Ice, Tile::Entrance, {HashFrameDataNameConstexpr("entrance"), 0, 0}, 1},
    {TileSet::Ice, Tile::Exit, {HashFrameDataNameConstexpr("exit"), 0, 0}, 1},

    {TileSet::Jungle, Tile::Air, {HashFrameDataNameConstexpr("jungle_air_0"), HashFrameDataNameConstexpr("jungle_air_1"), HashFrameDataNameConstexpr("jungle_air_2")}, 3},
    {TileSet::Jungle, Tile::Dirt, {HashFrameDataNameConstexpr("jungle_dirt_0"), HashFrameDataNameConstexpr("jungle_dirt_1"), HashFrameDataNameConstexpr("jungle_dirt_2")}, 3},
    {TileSet::Jungle, Tile::Gold, {HashFrameDataNameConstexpr("jungle_gold_0"), 0, 0}, 1},
    {TileSet::Jungle, Tile::Block, {HashFrameDataNameConstexpr("jungle_block_0"), 0, 0}, 1},
    {TileSet::Jungle, Tile::LadderTop, {HashFrameDataNameConstexpr("ladder_top_0"), 0, 0}, 1},
    {TileSet::Jungle, Tile::Ladder, {HashFrameDataNameConstexpr("ladder_0"), 0, 0}, 1},
    {TileSet::Jungle, Tile::Spikes, {HashFrameDataNameConstexpr("spikes_0"), 0, 0}, 1},
    {TileSet::Jungle, Tile::Rope, {HashFrameDataNameConstexpr("rope"), 0, 0}, 1},
    {TileSet::Jungle, Tile::Entrance, {HashFrameDataNameConstexpr("entrance"), 0, 0}, 1},
    {TileSet::Jungle, Tile::Exit, {HashFrameDataNameConstexpr("exit"), 0, 0}, 1},

    {TileSet::Temple, Tile::Air, {HashFrameDataNameConstexpr("temple_air_0"), HashFrameDataNameConstexpr("temple_air_1"), HashFrameDataNameConstexpr("temple_air_2")}, 3},
    {TileSet::Temple, Tile::Dirt, {HashFrameDataNameConstexpr("temple_dirt_0"), HashFrameDataNameConstexpr("temple_dirt_1"), HashFrameDataNameConstexpr("temple_dirt_2")}, 3},
    {TileSet::Temple, Tile::Gold, {HashFrameDataNameConstexpr("temple_gold"), 0, 0}, 1},
    {TileSet::Temple, Tile::Block, {HashFrameDataNameConstexpr("temple_block_0"), 0, 0}, 1},
    {TileSet::Temple, Tile::LadderTop, {HashFrameDataNameConstexpr("ladder_top_0"), 0, 0}, 1},
    {TileSet::Temple, Tile::Ladder, {HashFrameDataNameConstexpr("ladder_0"), 0, 0}, 1},
    {TileSet::Temple, Tile::Spikes, {HashFrameDataNameConstexpr("spikes_0"), 0, 0}, 1},
    {TileSet::Temple, Tile::Rope, {HashFrameDataNameConstexpr("rope"), 0, 0}, 1},
    {TileSet::Temple, Tile::Entrance, {HashFrameDataNameConstexpr("entrance"), 0, 0}, 1},
    {TileSet::Temple, Tile::Exit, {HashFrameDataNameConstexpr("exit"), 0, 0}, 1},

    {TileSet::Boss, Tile::Air, {HashFrameDataNameConstexpr("boss_air_0"), HashFrameDataNameConstexpr("boss_air_1"), HashFrameDataNameConstexpr("boss_air_2")}, 3},
    {TileSet::Boss, Tile::Dirt, {HashFrameDataNameConstexpr("boss_dirt_0"), HashFrameDataNameConstexpr("boss_dirt_1"), HashFrameDataNameConstexpr("boss_dirt_2")}, 3},
    {TileSet::Boss, Tile::Gold, {HashFrameDataNameConstexpr("boss_gold"), 0, 0}, 1},
    {TileSet::Boss, Tile::Block, {HashFrameDataNameConstexpr("boss_block_0"), 0, 0}, 1},
    {TileSet::Boss, Tile::LadderTop, {HashFrameDataNameConstexpr("ladder_top_0"), 0, 0}, 1},
    {TileSet::Boss, Tile::Ladder, {HashFrameDataNameConstexpr("ladder_0"), 0, 0}, 1},
    {TileSet::Boss, Tile::Spikes, {HashFrameDataNameConstexpr("spikes_0"), 0, 0}, 1},
    {TileSet::Boss, Tile::Rope, {HashFrameDataNameConstexpr("rope"), 0, 0}, 1},
    {TileSet::Boss, Tile::Entrance, {HashFrameDataNameConstexpr("entrance"), 0, 0}, 1},
    {TileSet::Boss, Tile::Exit, {HashFrameDataNameConstexpr("exit"), 0, 0}, 1},
}};

const FrameData& RequireSingleFrame(
    const FrameDataDb& frame_data_db,
    const std::unordered_map<std::uint32_t, std::size_t>& animation_index_by_hash,
    std::uint32_t name_hash
) {
    const auto found = animation_index_by_hash.find(name_hash);
    if (found == animation_index_by_hash.end()) {
        throw std::runtime_error("TileSourceData build error: mapped frame name hash not found");
    }

    const FrameDataAnimation& animation = frame_data_db.animations[found->second];
    if (animation.frame_indices.empty()) {
        throw std::runtime_error("TileSourceData build error: mapped animation has no frames");
    }
    return frame_data_db.frames[animation.frame_indices.front()];
}

std::uint32_t ResolveImageId(
    TileSourceDb& tile_source_db,
    std::unordered_map<std::string, std::uint32_t>& image_id_by_path,
    const std::string& path
) {
    const auto found = image_id_by_path.find(path);
    if (found != image_id_by_path.end()) {
        return found->second;
    }

    const std::uint32_t image_id = static_cast<std::uint32_t>(tile_source_db.image_paths.size());
    tile_source_db.image_paths.push_back(path);
    image_id_by_path.insert({path, image_id});
    return image_id;
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

std::uint32_t HashFrameDataName(const std::string& name) {
    return HashFrameDataNameConstexpr(name);
}

TileSourceDb BuildTileSourceDb(const FrameDataDb& frame_data_db) {
    TileSourceDb tile_source_db;
    tile_source_db.spans.resize(kTileSetCount * kTileCount);

    std::unordered_map<std::uint32_t, std::size_t> animation_index_by_hash;
    animation_index_by_hash.reserve(frame_data_db.animations.size());
    for (std::size_t i = 0; i < frame_data_db.animations.size(); ++i) {
        const std::uint32_t name_hash = HashFrameDataName(frame_data_db.animations[i].name);
        const auto found = animation_index_by_hash.find(name_hash);
        if (found != animation_index_by_hash.end() &&
            frame_data_db.animations[found->second].name != frame_data_db.animations[i].name) {
            throw std::runtime_error("TileSourceData build error: animation name hash collision");
        }
        animation_index_by_hash[name_hash] = i;
    }

    std::unordered_map<std::string, std::uint32_t> image_id_by_path;
    for (const TileSourceNameGroup& group : kTileSourceNameGroups) {
        TileSourceSpan& span =
            tile_source_db.spans[TileSourceSlotIndex(group.tile_set, group.tile)];
        span.first_source_index = static_cast<std::uint32_t>(tile_source_db.sources.size());
        span.source_count = group.count;

        for (std::uint32_t i = 0; i < group.count; ++i) {
            const FrameData& frame_data =
                RequireSingleFrame(frame_data_db, animation_index_by_hash, group.name_hashes[i]);
            const std::uint32_t image_id =
                ResolveImageId(tile_source_db, image_id_by_path, frame_data.path);
            tile_source_db.sources.push_back(TileSourceData{
                .image_id = image_id,
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
    if (image_index >= graphics.tile_source_images.size()) {
        return nullptr;
    }
    return graphics.tile_source_images[image_index];
}

} // namespace splonks

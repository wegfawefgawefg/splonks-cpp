#include "tile_contact_data.hpp"

#include "tile_source_data.hpp"

namespace splonks {

namespace {

constexpr std::size_t TileToIndex(Tile tile) {
    return static_cast<std::size_t>(tile);
}

} // namespace

TileContactDb BuildTileContactDb(const TileSourceDb& tile_source_db) {
    TileContactDb tile_contact_db;
    tile_contact_db.contacts.reserve(tile_source_db.sources.size());
    tile_contact_db.tile_spans.resize(tile_source_db.tile_spans.size());

    for (std::size_t tile_index = 0; tile_index < tile_source_db.tile_spans.size(); ++tile_index) {
        const TileSourceSpan& source_span = tile_source_db.tile_spans[tile_index];
        if (source_span.source_count == 0) {
            continue;
        }

        TileContactSpan& contact_span = tile_contact_db.tile_spans[tile_index];
        contact_span.first_contact_index = static_cast<std::uint32_t>(tile_contact_db.contacts.size());
        contact_span.contact_count = source_span.source_count;

        for (std::uint32_t i = 0; i < source_span.source_count; ++i) {
            const std::size_t source_index =
                static_cast<std::size_t>(source_span.first_source_index + i);
            if (source_index >= tile_source_db.sources.size()) {
                continue;
            }
            tile_contact_db.contacts.push_back(TileContactData{
                .cbox = tile_source_db.sources[source_index].cbox,
            });
        }
    }

    return tile_contact_db;
}

const TileContactData* GetTileContactData(
    const TileContactDb& tile_contact_db,
    Tile tile,
    const IVec2& tile_pos
) {
    if (tile == Tile::Air) {
        return nullptr;
    }
    const std::size_t tile_index = TileToIndex(tile);
    if (tile_index >= tile_contact_db.tile_spans.size()) {
        return nullptr;
    }

    const TileContactSpan& span = tile_contact_db.tile_spans[tile_index];
    if (span.contact_count == 0 ||
        span.first_contact_index >= tile_contact_db.contacts.size()) {
        return nullptr;
    }

    std::uint32_t variation = 0;
    if (span.contact_count > 1) {
        const std::uint32_t seed =
            static_cast<std::uint32_t>(static_cast<std::uint32_t>(tile_pos.x) * 73856093U) ^
            static_cast<std::uint32_t>(static_cast<std::uint32_t>(tile_pos.y) * 19349663U);
        variation = seed % span.contact_count;
    }

    const std::size_t contact_index =
        static_cast<std::size_t>(span.first_contact_index + variation);
    if (contact_index >= tile_contact_db.contacts.size()) {
        return nullptr;
    }
    return &tile_contact_db.contacts[contact_index];
}

} // namespace splonks

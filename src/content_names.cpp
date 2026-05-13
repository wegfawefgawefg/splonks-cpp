#include "content_names.hpp"

#include "ent/spec.hpp"
#include "tile_spec.hpp"

#include <cctype>
#include <stdexcept>
#include <unordered_map>

namespace splonks {

namespace {

std::string NormalizeContentKey(std::string_view name) {
    std::string normalized;
    normalized.reserve(name.size());
    for (const char c : name) {
        if (std::isalnum(static_cast<unsigned char>(c)) == 0) {
            continue;
        }
        normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return normalized;
}

std::string DebugNameToContentName(std::string_view debug_name) {
    std::string content_name;
    content_name.reserve(debug_name.size() + 4);
    char previous = '\0';
    for (const char c : debug_name) {
        if (std::isalnum(static_cast<unsigned char>(c)) == 0) {
            previous = c;
            continue;
        }

        if (std::isupper(static_cast<unsigned char>(c)) != 0 && !content_name.empty() &&
            std::islower(static_cast<unsigned char>(previous)) != 0) {
            content_name.push_back('_');
        }
        content_name.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        previous = c;
    }
    return content_name;
}

const std::unordered_map<std::string, EntType>& EntContentNameMap() {
    static const std::unordered_map<std::string, EntType> kMap = [] {
        PopulateEntSpecsTable();

        std::unordered_map<std::string, EntType> map;
        map.reserve(kEntTypeCount);
        for (std::size_t type_index = 0; type_index < kEntTypeCount; ++type_index) {
            const EntType type = static_cast<EntType>(type_index);
            const std::string key = NormalizeContentKey(GetEntTypeName(type));
            if (key.empty()) {
                continue;
            }
            const auto result = map.insert({key, type});
            if (!result.second) {
                throw std::runtime_error("Duplicate ent content name: " +
                                         std::string(GetEntTypeName(type)));
            }
        }
        return map;
    }();
    return kMap;
}

const std::unordered_map<std::string, Tile>& TileContentNameMap() {
    static const std::unordered_map<std::string, Tile> kMap = [] {
        std::unordered_map<std::string, Tile> map;
        map.reserve(kTileCount);
        for (std::size_t tile_index = 0; tile_index < kTileCount; ++tile_index) {
            const Tile tile = static_cast<Tile>(tile_index);
            const std::string key = NormalizeContentKey(GetTileSpec(tile).debug_name);
            if (key.empty()) {
                continue;
            }
            const auto result = map.insert({key, tile});
            if (!result.second) {
                throw std::runtime_error("Duplicate tile content name: " +
                                         std::string(GetTileSpec(tile).debug_name));
            }
        }
        return map;
    }();
    return kMap;
}

} // namespace

std::optional<EntType> EntTypeFromContentName(std::string_view name) {
    const auto found = EntContentNameMap().find(NormalizeContentKey(name));
    if (found == EntContentNameMap().end()) {
        return std::nullopt;
    }
    return found->second;
}

std::optional<Tile> TileFromContentName(std::string_view name) {
    const auto found = TileContentNameMap().find(NormalizeContentKey(name));
    if (found == TileContentNameMap().end()) {
        return std::nullopt;
    }
    return found->second;
}

std::string ContentNameFromEntType(EntType ent_type) {
    PopulateEntSpecsTable();
    return DebugNameToContentName(GetEntTypeName(ent_type));
}

std::string ContentNameFromTile(Tile tile) {
    return DebugNameToContentName(GetTileSpec(tile).debug_name);
}

} // namespace splonks

#pragma once

#include "ent/core_types.hpp"
#include "tile.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace splonks {

std::optional<EntType> EntTypeFromContentName(std::string_view name);
std::optional<Tile> TileFromContentName(std::string_view name);
std::string ContentNameFromEntType(EntType ent_type);
std::string ContentNameFromTile(Tile tile);

} // namespace splonks

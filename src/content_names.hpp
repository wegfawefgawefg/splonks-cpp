#pragma once

#include "entity/core_types.hpp"
#include "tile.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace splonks {

std::optional<EntityType> EntityTypeFromContentName(std::string_view name);
std::optional<Tile> TileFromContentName(std::string_view name);
std::string ContentNameFromEntityType(EntityType entity_type);
std::string ContentNameFromTile(Tile tile);

} // namespace splonks

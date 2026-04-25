#pragma once

#include "quest.hpp"

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace splonks::quest_parse {

std::string Trim(const std::string& value);
int IndentOf(const std::string& line);
bool IsBlankOrComment(const std::string& line);
std::pair<std::string, std::string> SplitKeyValue(
    const std::string& line,
    const std::string& path,
    int line_number
);
int ParseInt(const std::string& value, const std::string& path, int line_number);
bool ParseBool(const std::string& value, const std::string& path, int line_number);
UVec2 ParseSize(const std::string& value, const std::string& path, int line_number);
std::vector<std::string> ReadLines(const std::string& path);
std::filesystem::path ResolveQuestPath(
    const std::string& quest_root_path,
    const std::string& relative_path
);
std::string StripQuotes(const std::string& value);
EntityType ParseEntityTypeOrThrow(const std::string& value, const std::string& path, int line_number);
Tile ParseTileOrThrow(const std::string& value, const std::string& path, int line_number);
std::vector<Tile> ParseTileList(const std::string& value, const std::string& path, int line_number);

} // namespace splonks::quest_parse

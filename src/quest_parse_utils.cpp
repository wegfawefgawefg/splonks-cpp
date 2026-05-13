#include "quest_parse_utils.hpp"

#include "content_names.hpp"

#include <cctype>
#include <fstream>
#include <stdexcept>

namespace splonks::quest_parse {

std::string Trim(const std::string& value) {
    std::size_t begin = 0;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
        begin += 1;
    }

    std::size_t end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        end -= 1;
    }

    return value.substr(begin, end - begin);
}

int IndentOf(const std::string& line) {
    int indent = 0;
    while (indent < static_cast<int>(line.size()) &&
           line[static_cast<std::size_t>(indent)] == ' ') {
        indent += 1;
    }
    return indent;
}

bool IsBlankOrComment(const std::string& line) {
    const std::string trimmed = Trim(line);
    return trimmed.empty() || trimmed.rfind("#", 0) == 0;
}

std::pair<std::string, std::string> SplitKeyValue(
    const std::string& line,
    const std::string& path,
    int line_number
) {
    const std::size_t colon = line.find(':');
    if (colon == std::string::npos) {
        throw std::runtime_error(path + ":" + std::to_string(line_number) +
                                 ": expected key/value pair");
    }
    return {Trim(line.substr(0, colon)), Trim(line.substr(colon + 1))};
}

int ParseInt(const std::string& value, const std::string& path, int line_number) {
    try {
        std::size_t parsed = 0;
        const int result = std::stoi(value, &parsed);
        if (parsed != value.size()) {
            throw std::invalid_argument("trailing characters");
        }
        return result;
    } catch (const std::exception&) {
        throw std::runtime_error(path + ":" + std::to_string(line_number) +
                                 ": invalid integer: " + value);
    }
}

bool ParseBool(const std::string& value, const std::string& path, int line_number) {
    if (value == "true") {
        return true;
    }
    if (value == "false") {
        return false;
    }
    throw std::runtime_error(path + ":" + std::to_string(line_number) + ": invalid bool: " + value);
}

UVec2 ParseSize(const std::string& value, const std::string& path, int line_number) {
    const std::string trimmed = Trim(value);
    if (trimmed.size() < 5 || trimmed.front() != '[' || trimmed.back() != ']') {
        throw std::runtime_error(path + ":" + std::to_string(line_number) +
                                 ": expected size like [10, 8]");
    }

    const std::string body = trimmed.substr(1, trimmed.size() - 2);
    const std::size_t comma = body.find(',');
    if (comma == std::string::npos) {
        throw std::runtime_error(path + ":" + std::to_string(line_number) +
                                 ": expected size like [10, 8]");
    }

    const int x = ParseInt(Trim(body.substr(0, comma)), path, line_number);
    const int y = ParseInt(Trim(body.substr(comma + 1)), path, line_number);
    if (x <= 0 || y <= 0) {
        throw std::runtime_error(path + ":" + std::to_string(line_number) +
                                 ": size must be positive");
    }
    return UVec2::New(static_cast<unsigned int>(x), static_cast<unsigned int>(y));
}

std::vector<std::string> ReadLines(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Failed to open YAML file: " + path);
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    return lines;
}

std::filesystem::path ResolveQuestPath(
    const std::string& quest_root_path,
    const std::string& relative_path
) {
    return std::filesystem::path(quest_root_path) / relative_path;
}

std::string StripQuotes(const std::string& value) {
    const std::string trimmed = Trim(value);
    if (trimmed.size() >= 2 && ((trimmed.front() == '"' && trimmed.back() == '"') ||
                                (trimmed.front() == '\'' && trimmed.back() == '\''))) {
        return trimmed.substr(1, trimmed.size() - 2);
    }
    return trimmed;
}

EntType ParseEntTypeOrThrow(const std::string& value, const std::string& path, int line_number) {
    const std::optional<EntType> ent_type = EntTypeFromContentName(value);
    if (!ent_type.has_value()) {
        throw std::runtime_error(path + ":" + std::to_string(line_number) +
                                 ": unknown ent type: " + value);
    }
    return *ent_type;
}

Tile ParseTileOrThrow(const std::string& value, const std::string& path, int line_number) {
    const std::optional<Tile> tile = TileFromContentName(value);
    if (!tile.has_value()) {
        throw std::runtime_error(path + ":" + std::to_string(line_number) +
                                 ": unknown tile: " + value);
    }
    return *tile;
}

std::vector<Tile> ParseTileList(const std::string& value, const std::string& path, int line_number) {
    const std::string trimmed = Trim(value);
    if (trimmed.size() < 2 || trimmed.front() != '[' || trimmed.back() != ']') {
        throw std::runtime_error(path + ":" + std::to_string(line_number) +
                                 ": expected tile list like [cave_air0, cave_air1]");
    }

    std::vector<Tile> tiles;
    std::string body = trimmed.substr(1, trimmed.size() - 2);
    while (!body.empty()) {
        const std::size_t comma = body.find(',');
        const std::string name = StripQuotes(Trim(body.substr(0, comma)));
        if (!name.empty()) {
            tiles.push_back(ParseTileOrThrow(name, path, line_number));
        }
        if (comma == std::string::npos) {
            break;
        }
        body = body.substr(comma + 1);
    }
    if (tiles.empty()) {
        throw std::runtime_error(path + ":" + std::to_string(line_number) +
                                 ": tile list cannot be empty");
    }
    return tiles;
}

} // namespace splonks::quest_parse

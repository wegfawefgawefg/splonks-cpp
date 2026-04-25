#include "stage_gen/room_template_loader.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace splonks::stage_gen {

namespace {

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

std::pair<std::string, std::string> SplitKeyValue(
    const std::string& line,
    const std::string& path,
    int line_number
) {
    const std::size_t colon = line.find(':');
    if (colon == std::string::npos) {
        throw std::runtime_error(
            path + ":" + std::to_string(line_number) + ": expected key/value pair"
        );
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
        throw std::runtime_error(
            path + ":" + std::to_string(line_number) + ": invalid integer: " + value
        );
    }
}

UVec2 ParseSize(const std::string& value, const std::string& path, int line_number) {
    const std::string trimmed = Trim(value);
    if (trimmed.size() < 5 || trimmed.front() != '[' || trimmed.back() != ']') {
        throw std::runtime_error(
            path + ":" + std::to_string(line_number) + ": expected size like [10, 8]"
        );
    }

    const std::string body = trimmed.substr(1, trimmed.size() - 2);
    const std::size_t comma = body.find(',');
    if (comma == std::string::npos) {
        throw std::runtime_error(
            path + ":" + std::to_string(line_number) + ": expected size like [10, 8]"
        );
    }

    const int x = ParseInt(Trim(body.substr(0, comma)), path, line_number);
    const int y = ParseInt(Trim(body.substr(comma + 1)), path, line_number);
    if (x <= 0 || y <= 0) {
        throw std::runtime_error(
            path + ":" + std::to_string(line_number) + ": room size must be positive"
        );
    }

    return UVec2::New(static_cast<unsigned int>(x), static_cast<unsigned int>(y));
}

std::string StripGridIndent(const std::string& line) {
    if (line.rfind("  ", 0) == 0) {
        return line.substr(2);
    }
    return Trim(line);
}

RoomTemplate LoadRoomTemplateFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Failed to open room template: " + path.string());
    }

    RoomTemplate room;
    room.source_path = path.string();

    bool in_properties = false;
    bool in_grid = false;
    std::vector<std::string> grid_rows;
    std::string line;
    int line_number = 0;
    while (std::getline(file, line)) {
        line_number += 1;
        if (in_grid) {
            if (!Trim(line).empty()) {
                grid_rows.push_back(StripGridIndent(line));
            }
            continue;
        }

        if (Trim(line).empty() || Trim(line).rfind("#", 0) == 0) {
            continue;
        }

        if (in_properties && line.rfind("  ", 0) == 0) {
            const auto [key, value] = SplitKeyValue(line, path.string(), line_number);
            room.properties[key] = value;
            continue;
        }
        in_properties = false;

        const auto [key, value] = SplitKeyValue(line, path.string(), line_number);
        if (key == "id") {
            room.id = value;
        } else if (key == "pool") {
            room.pool = value;
        } else if (key == "weight") {
            room.weight = ParseInt(value, path.string(), line_number);
        } else if (key == "size") {
            room.size = ParseSize(value, path.string(), line_number);
        } else if (key == "properties") {
            if (!value.empty()) {
                throw std::runtime_error(
                    path.string() + ":" + std::to_string(line_number) +
                    ": properties must be a block"
                );
            }
            in_properties = true;
        } else if (key == "grid") {
            if (value != "|") {
                throw std::runtime_error(
                    path.string() + ":" + std::to_string(line_number) + ": grid must use block scalar |"
                );
            }
            in_grid = true;
        } else {
            throw std::runtime_error(
                path.string() + ":" + std::to_string(line_number) + ": unknown room field: " + key
            );
        }
    }

    if (room.id.empty()) {
        throw std::runtime_error(path.string() + ": missing id");
    }
    if (room.pool.empty()) {
        throw std::runtime_error(path.string() + ": missing pool");
    }
    if (room.size.x == 0 || room.size.y == 0) {
        throw std::runtime_error(path.string() + ": missing size");
    }
    if (grid_rows.size() != room.size.y) {
        throw std::runtime_error(path.string() + ": grid row count does not match size");
    }

    std::ostringstream grid;
    for (const std::string& row : grid_rows) {
        if (row.size() != room.size.x) {
            throw std::runtime_error(path.string() + ": grid row width does not match size");
        }
        grid << row;
    }
    room.grid = grid.str();
    return room;
}

} // namespace

std::vector<RoomTemplate> LoadRoomTemplatePool(const std::string& directory_path) {
    const std::filesystem::path directory(directory_path);
    if (!std::filesystem::exists(directory)) {
        return {};
    }
    if (!std::filesystem::is_directory(directory)) {
        throw std::runtime_error("Room template path is not a directory: " + directory_path);
    }

    std::vector<std::filesystem::path> paths;
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const std::filesystem::path path = entry.path();
        if (path.extension() == ".yaml") {
            paths.push_back(path);
        }
    }
    std::sort(paths.begin(), paths.end());

    std::vector<RoomTemplate> rooms;
    rooms.reserve(paths.size());
    for (const std::filesystem::path& path : paths) {
        rooms.push_back(LoadRoomTemplateFile(path));
    }
    return rooms;
}

} // namespace splonks::stage_gen

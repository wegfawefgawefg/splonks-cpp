#pragma once

#include "math_types.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace splonks::stage_gen {

struct RoomTemplate {
    std::string id;
    std::string pool;
    int weight = 1;
    UVec2 size = UVec2::New(0, 0);
    std::unordered_map<std::string, std::string> properties;
    std::string grid;
    std::string source_path;
};

std::vector<RoomTemplate> LoadRoomTemplatePool(const std::string& directory_path);

} // namespace splonks::stage_gen

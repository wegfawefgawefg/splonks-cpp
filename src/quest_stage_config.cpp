#include "quest.hpp"

#include "quest_parse_utils.hpp"

#include <stdexcept>

namespace splonks {

namespace {

void ValidateStageConfig(const StageConfig& config, const std::string& path) {
    if (config.id.empty()) {
        throw std::runtime_error(path + ": missing id");
    }
    if (config.generator.empty()) {
        throw std::runtime_error(path + ": missing generator");
    }
    if (config.room_size.x == 0 || config.room_size.y == 0) {
        throw std::runtime_error(path + ": missing room_size");
    }
    if (config.layout_size.x == 0 || config.layout_size.y == 0) {
        throw std::runtime_error(path + ": missing layout_size");
    }
    if ((config.path_layout_size.x == 0) != (config.path_layout_size.y == 0)) {
        throw std::runtime_error(path + ": path_layout_size must specify both dimensions");
    }
    if (config.path_layout_size.x != 0 &&
        (config.path_layout_size.x > config.layout_size.x ||
         config.path_layout_size.y > config.layout_size.y)) {
        throw std::runtime_error(path + ": path_layout_size must fit within layout_size");
    }
}

} // namespace

int StagePassConfig::GetInt(std::string_view key, int fallback) const {
    if (const auto it = properties.find(std::string(key)); it != properties.end()) {
        try {
            return std::stoi(it->second);
        } catch (const std::exception&) {
            return fallback;
        }
    }
    return fallback;
}

bool StagePassConfig::GetBool(std::string_view key, bool fallback) const {
    if (const auto it = properties.find(std::string(key)); it != properties.end()) {
        if (it->second == "true") {
            return true;
        }
        if (it->second == "false") {
            return false;
        }
    }
    return fallback;
}

StageConfig LoadStageConfig(
    const std::string& quest_root_path,
    const std::string& stage_file_path
) {
    using namespace quest_parse;

    const std::string path = ResolveQuestPath(quest_root_path, stage_file_path).string();
    const std::vector<std::string> lines = ReadLines(path);
    StageConfig config;
    enum class Block {
        None,
        RoomPools,
        LayoutPasses,
        StagePasses,
        Properties,
    };
    Block block = Block::None;
    Block active_pass_block = Block::None;
    StagePassConfig* current_pass = nullptr;

    for (std::size_t i = 0; i < lines.size(); ++i) {
        const std::string& line = lines[i];
        const int line_number = static_cast<int>(i + 1);
        if (IsBlankOrComment(line)) {
            continue;
        }

        const int indent = IndentOf(line);
        const std::string trimmed = Trim(line);
        if (indent == 0) {
            current_pass = nullptr;
            block = Block::None;
            if (trimmed == "room_pools:") {
                block = Block::RoomPools;
                continue;
            }
            if (trimmed == "layout_passes:") {
                block = Block::LayoutPasses;
                active_pass_block = Block::LayoutPasses;
                continue;
            }
            if (trimmed == "stage_passes:") {
                block = Block::StagePasses;
                active_pass_block = Block::StagePasses;
                continue;
            }
            const auto [key, value] = SplitKeyValue(trimmed, path, line_number);
            if (key == "id") {
                config.id = value;
            } else if (key == "title") {
                config.title = value;
            } else if (key == "generator") {
                config.generator = value;
            } else if (key == "room_size") {
                config.room_size = ParseSize(value, path, line_number);
            } else if (key == "layout_size") {
                config.layout_size = ParseSize(value, path, line_number);
            } else if (key == "path_layout_size") {
                config.path_layout_size = ParseSize(value, path, line_number);
            } else if (key == "glyphs") {
                config.glyphs_path = value;
            } else if (key == "border_tile") {
                config.border_tile = ParseTileOrThrow(value, path, line_number);
            } else if (key == "backwall_tiles") {
                config.backwall_tiles = ParseTileList(value, path, line_number);
            } else if (key == "block_animation") {
                config.block_animation_id = HashFrameDataId(value);
            } else {
                throw std::runtime_error(path + ":" + std::to_string(line_number) +
                                         ": unknown stage config field: " + key);
            }
            continue;
        }

        if (block == Block::RoomPools && indent == 2) {
            const auto [key, value] = SplitKeyValue(trimmed, path, line_number);
            config.room_pools[key] = value;
            continue;
        }

        const bool in_pass_list = block == Block::LayoutPasses || block == Block::StagePasses ||
                                  block == Block::Properties;
        if (in_pass_list && indent == 2 && trimmed.rfind("- ", 0) == 0) {
            block = active_pass_block;
            std::vector<StagePassConfig>& passes = active_pass_block == Block::StagePasses
                                                       ? config.stage_passes
                                                       : config.layout_passes;
            passes.push_back(StagePassConfig{});
            current_pass = &passes.back();
            const auto [key, value] = SplitKeyValue(trimmed.substr(2), path, line_number);
            if (key != "name") {
                throw std::runtime_error(path + ":" + std::to_string(line_number) +
                                         ": pass entry must start with name");
            }
            current_pass->name = value;
            continue;
        }

        if ((block == Block::LayoutPasses || block == Block::StagePasses ||
             block == Block::Properties) &&
            current_pass != nullptr) {
            if (indent == 4) {
                const auto [key, value] = SplitKeyValue(trimmed, path, line_number);
                if (key == "enabled") {
                    current_pass->enabled = ParseBool(value, path, line_number);
                } else if (key == "properties") {
                    if (!value.empty()) {
                        throw std::runtime_error(path + ":" + std::to_string(line_number) +
                                                 ": properties must be a block");
                    }
                    block = Block::Properties;
                } else {
                    throw std::runtime_error(path + ":" + std::to_string(line_number) +
                                             ": unknown pass field: " + key);
                }
                continue;
            }
            if (block == Block::Properties && indent == 6) {
                const auto [key, value] = SplitKeyValue(trimmed, path, line_number);
                current_pass->properties[key] = value;
                continue;
            }
        }

        throw std::runtime_error(path + ":" + std::to_string(line_number) +
                                 ": unsupported stage config YAML shape");
    }

    ValidateStageConfig(config, path);
    return config;
}

} // namespace splonks

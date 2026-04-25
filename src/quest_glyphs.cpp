#include "quest.hpp"

#include "quest_parse_utils.hpp"

#include <stdexcept>

namespace splonks {

namespace {

char ParseGlyphChar(const std::string& value, const std::string& path, int line_number) {
    const std::string stripped = quest_parse::StripQuotes(value);
    if (stripped.size() != 1) {
        throw std::runtime_error(path + ":" + std::to_string(line_number) +
                                 ": glyph char must be one character: " + value);
    }
    return stripped[0];
}

void ValidateGlyphRule(const GlyphRule& rule, const std::string& path) {
    if (rule.glyph == '\0') {
        throw std::runtime_error(path + ": glyph rule missing char");
    }
    if (!rule.tile.has_value() && rule.action.empty() && rule.patch_pool.empty() &&
        rule.spawn == EntityType::None && rule.spawn_chance == EntityType::None &&
        rule.spawn_random.empty()) {
        throw std::runtime_error(path + ": glyph rule has no behavior for glyph " +
                                 std::string(1, rule.glyph));
    }
    if (rule.chance_denominator <= 0) {
        throw std::runtime_error(path + ": glyph rule chance_denominator must be positive for glyph " +
                                 std::string(1, rule.glyph));
    }
    if (!rule.patch_pool.empty() &&
        (rule.tile.has_value() || !rule.action.empty() || rule.spawn != EntityType::None ||
         rule.spawn_chance != EntityType::None || !rule.spawn_random.empty())) {
        throw std::runtime_error(path + ": patch_pool glyph cannot combine with other behavior for glyph " +
                                 std::string(1, rule.glyph));
    }
    if (rule.spawn != EntityType::None && rule.spawn_chance != EntityType::None) {
        throw std::runtime_error(path + ": glyph cannot use both spawn and spawn_chance for glyph " +
                                 std::string(1, rule.glyph));
    }
}

} // namespace

const GlyphRule* GlyphMap::Find(char glyph) const {
    const auto it = rules.find(glyph);
    return it == rules.end() ? nullptr : &it->second;
}

GlyphMap LoadGlyphMap(const std::string& quest_root_path, const std::string& glyph_file_path) {
    using namespace quest_parse;

    const std::string path = ResolveQuestPath(quest_root_path, glyph_file_path).string();
    const std::vector<std::string> lines = ReadLines(path);
    GlyphMap map;
    GlyphRule current_rule;
    bool has_rule = false;
    bool in_glyphs = false;
    bool in_spawn_random = false;
    WeightedEntityEntry* current_random_entry = nullptr;

    const auto finish_rule = [&]() {
        if (!has_rule) {
            return;
        }
        ValidateGlyphRule(current_rule, path);
        if (map.rules.contains(current_rule.glyph)) {
            throw std::runtime_error(
                path + ": duplicate glyph rule: " + std::string(1, current_rule.glyph));
        }
        map.rules[current_rule.glyph] = current_rule;
        current_rule = GlyphRule{};
        has_rule = false;
        in_spawn_random = false;
        current_random_entry = nullptr;
    };

    for (std::size_t i = 0; i < lines.size(); ++i) {
        const std::string& line = lines[i];
        const int line_number = static_cast<int>(i + 1);
        if (IsBlankOrComment(line)) {
            continue;
        }

        const int indent = IndentOf(line);
        const std::string trimmed = Trim(line);
        if (indent == 0) {
            if (trimmed == "glyphs:") {
                in_glyphs = true;
                continue;
            }
            throw std::runtime_error(path + ":" + std::to_string(line_number) +
                                     ": unknown glyph file field");
        }
        if (!in_glyphs) {
            throw std::runtime_error(path + ":" + std::to_string(line_number) +
                                     ": glyph entry before glyphs block");
        }

        if (indent == 2 && trimmed.rfind("- ", 0) == 0) {
            finish_rule();
            has_rule = true;
            const auto [key, value] = SplitKeyValue(trimmed.substr(2), path, line_number);
            if (key != "char") {
                throw std::runtime_error(path + ":" + std::to_string(line_number) +
                                         ": glyph entry must start with char");
            }
            current_rule.glyph = ParseGlyphChar(value, path, line_number);
            continue;
        }
        if (!has_rule) {
            throw std::runtime_error(path + ":" + std::to_string(line_number) +
                                     ": glyph field before char");
        }
        if (indent == 4) {
            current_random_entry = nullptr;
            const auto [key, value] = SplitKeyValue(trimmed, path, line_number);
            if (key == "tile") {
                current_rule.tile = ParseTileOrThrow(value, path, line_number);
            } else if (key == "action") {
                current_rule.action = value;
            } else if (key == "patch_pool") {
                current_rule.patch_pool = value;
            } else if (key == "spawn") {
                current_rule.spawn = ParseEntityTypeOrThrow(value, path, line_number);
            } else if (key == "spawn_chance") {
                current_rule.spawn_chance = ParseEntityTypeOrThrow(value, path, line_number);
            } else if (key == "chance_denominator") {
                current_rule.chance_denominator = ParseInt(value, path, line_number);
            } else if (key == "exit_id") {
                current_rule.exit_id = value;
            } else if (key == "spawn_random") {
                if (!value.empty()) {
                    throw std::runtime_error(path + ":" + std::to_string(line_number) +
                                             ": spawn_random must be a block");
                }
                in_spawn_random = true;
            } else {
                throw std::runtime_error(path + ":" + std::to_string(line_number) +
                                         ": unknown glyph field: " + key);
            }
            continue;
        }
        if (in_spawn_random && indent == 6 && trimmed.rfind("- ", 0) == 0) {
            const auto [key, value] = SplitKeyValue(trimmed.substr(2), path, line_number);
            if (key != "entity") {
                throw std::runtime_error(path + ":" + std::to_string(line_number) +
                                         ": spawn_random entry must start with entity");
            }
            current_rule.spawn_random.push_back(WeightedEntityEntry{
                .entity_type = ParseEntityTypeOrThrow(value, path, line_number),
                .weight = 1,
            });
            current_random_entry = &current_rule.spawn_random.back();
            continue;
        }
        if (in_spawn_random && indent == 8 && current_random_entry != nullptr) {
            const auto [key, value] = SplitKeyValue(trimmed, path, line_number);
            if (key != "weight") {
                throw std::runtime_error(path + ":" + std::to_string(line_number) +
                                         ": unknown spawn_random field: " + key);
            }
            current_random_entry->weight = ParseInt(value, path, line_number);
            continue;
        }
        throw std::runtime_error(path + ":" + std::to_string(line_number) +
                                 ": unsupported glyph YAML shape");
    }
    finish_rule();
    return map;
}

} // namespace splonks

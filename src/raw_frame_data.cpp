#include "raw_frame_data.hpp"

#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace splonks {

namespace {

enum class NestedField {
    None,
    Aabb,
    Offset,
    Center,
    Pbox,
    Cbox,
};

std::string Trim(const std::string& value) {
    std::size_t start = 0;
    while (start < value.size() &&
           std::isspace(static_cast<unsigned char>(value[start])) != 0) {
        start += 1;
    }

    std::size_t end = value.size();
    while (end > start &&
           std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        end -= 1;
    }

    return value.substr(start, end - start);
}

int CountIndent(const std::string& line) {
    int indent = 0;
    while (indent < static_cast<int>(line.size()) &&
           line[static_cast<std::size_t>(indent)] == ' ') {
        indent += 1;
    }
    return indent;
}

std::string StripQuotes(const std::string& value) {
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        return value.substr(1, value.size() - 2);
    }
    if (value.size() >= 2 && value.front() == '\'' && value.back() == '\'') {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

std::string ParseTrimmedString(const std::string& value) {
    return Trim(StripQuotes(value));
}

std::pair<std::string, std::string> SplitKeyValue(
    const std::string& text,
    const std::string& yaml_path,
    int line_number
) {
    const std::size_t colon = text.find(':');
    if (colon == std::string::npos) {
        throw std::runtime_error(
            yaml_path + ":" + std::to_string(line_number) + ": expected key/value pair"
        );
    }

    const std::string key = Trim(text.substr(0, colon));
    const std::string value = Trim(text.substr(colon + 1));
    return {key, value};
}

int ParseInt(const std::string& value, const std::string& yaml_path, int line_number) {
    try {
        std::size_t parsed = 0;
        const int result = std::stoi(value, &parsed);
        if (parsed != value.size()) {
            throw std::runtime_error("");
        }
        return result;
    } catch (...) {
        throw std::runtime_error(
            yaml_path + ":" + std::to_string(line_number) + ": invalid integer: " + value
        );
    }
}

bool ParseBool(const std::string& value, const std::string& yaml_path, int line_number) {
    if (value == "true") {
        return true;
    }
    if (value == "false") {
        return false;
    }
    throw std::runtime_error(
        yaml_path + ":" + std::to_string(line_number) + ": invalid boolean: " + value
    );
}

std::vector<std::string> ParseTags(
    const std::string& value,
    const std::string& yaml_path,
    int line_number
) {
    const std::string trimmed = Trim(value);
    if (trimmed == "[]") {
        return {};
    }
    if (trimmed.size() < 2 || trimmed.front() != '[' || trimmed.back() != ']') {
        throw std::runtime_error(
            yaml_path + ":" + std::to_string(line_number) + ": invalid tags list"
        );
    }

    std::vector<std::string> tags;
    const std::string inner = trimmed.substr(1, trimmed.size() - 2);
    std::stringstream stream(inner);
    std::string part;
    while (std::getline(stream, part, ',')) {
        const std::string tag = StripQuotes(Trim(part));
        if (!tag.empty()) {
            tags.push_back(Trim(tag));
        }
    }
    return tags;
}

void AssignNestedInt(
    RawFrameData& frame_data,
    NestedField nested_field,
    const std::string& key,
    int value
) {
    switch (nested_field) {
    case NestedField::Aabb:
        if (key == "x") {
            frame_data.aabb.x = value;
        } else if (key == "y") {
            frame_data.aabb.y = value;
        } else if (key == "w") {
            frame_data.aabb.w = value;
        } else if (key == "h") {
            frame_data.aabb.h = value;
        }
        break;
    case NestedField::Offset:
        if (key == "x") {
            frame_data.offset.x = value;
        } else if (key == "y") {
            frame_data.offset.y = value;
        }
        break;
    case NestedField::Center:
        if (key == "x") {
            frame_data.center.x = value;
        } else if (key == "y") {
            frame_data.center.y = value;
        }
        break;
    case NestedField::Pbox:
        frame_data.has_pbox = true;
        if (key == "x") {
            frame_data.pbox.x = value;
        } else if (key == "y") {
            frame_data.pbox.y = value;
        } else if (key == "w") {
            frame_data.pbox.w = value;
        } else if (key == "h") {
            frame_data.pbox.h = value;
        }
        break;
    case NestedField::Cbox:
        frame_data.has_cbox = true;
        if (key == "x") {
            frame_data.cbox.x = value;
        } else if (key == "y") {
            frame_data.cbox.y = value;
        } else if (key == "w") {
            frame_data.cbox.w = value;
        } else if (key == "h") {
            frame_data.cbox.h = value;
        }
        break;
    case NestedField::None:
        break;
    }
}

void AssignFrameField(
    RawFrameData& frame_data,
    const std::string& key,
    const std::string& value,
    const std::string& yaml_path,
    int line_number
) {
    if (key == "path") {
        frame_data.path = ParseTrimmedString(value);
    } else if (key == "name") {
        frame_data.name = ParseTrimmedString(value);
    } else if (key == "frame") {
        frame_data.frame = ParseInt(value, yaml_path, line_number);
    } else if (key == "duration") {
        frame_data.duration = ParseInt(value, yaml_path, line_number);
    } else if (key == "tags") {
        frame_data.tags = ParseTags(value, yaml_path, line_number);
    } else if (key == "tile") {
        frame_data.tile = ParseBool(value, yaml_path, line_number);
    }
}

} // namespace

RawFrameDataFile LoadRawFrameDataFile(const std::string& yaml_path) {
    std::ifstream file(yaml_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open annotations file: " + yaml_path);
    }

    RawFrameDataFile result;
    RawFrameData current_frame_data;
    bool in_sprites = false;
    bool have_current_frame_data = false;
    NestedField nested_field = NestedField::None;

    std::string line;
    int line_number = 0;
    while (std::getline(file, line)) {
        line_number += 1;
        const std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed.front() == '#') {
            continue;
        }

        const int indent = CountIndent(line);
        if (trimmed == "sprites:") {
            in_sprites = true;
            nested_field = NestedField::None;
            continue;
        }
        if (!in_sprites) {
            continue;
        }

        if (indent == 2 && trimmed.rfind("- ", 0) == 0) {
            if (have_current_frame_data) {
                result.sprites.push_back(current_frame_data);
            }
            current_frame_data = RawFrameData{};
            have_current_frame_data = true;
            nested_field = NestedField::None;

            const std::string remainder = Trim(trimmed.substr(2));
            if (!remainder.empty()) {
                const auto [key, value] = SplitKeyValue(remainder, yaml_path, line_number);
                AssignFrameField(current_frame_data, key, value, yaml_path, line_number);
            }
            continue;
        }

        if (!have_current_frame_data) {
            throw std::runtime_error(
                yaml_path + ":" + std::to_string(line_number) +
                ": frame field encountered before sprite item"
            );
        }

        if (indent == 4) {
            const auto [key, value] = SplitKeyValue(trimmed, yaml_path, line_number);
            if (value.empty()) {
                if (key == "aabb") {
                    nested_field = NestedField::Aabb;
                } else if (key == "offset") {
                    nested_field = NestedField::Offset;
                } else if (key == "center") {
                    nested_field = NestedField::Center;
                } else if (key == "pbox") {
                    nested_field = NestedField::Pbox;
                } else if (key == "cbox") {
                    nested_field = NestedField::Cbox;
                } else {
                    nested_field = NestedField::None;
                }
            } else {
                nested_field = NestedField::None;
                AssignFrameField(current_frame_data, key, value, yaml_path, line_number);
            }
            continue;
        }

        if (indent == 6) {
            const auto [key, value] = SplitKeyValue(trimmed, yaml_path, line_number);
            if (nested_field == NestedField::None) {
                throw std::runtime_error(
                    yaml_path + ":" + std::to_string(line_number) +
                    ": nested field without parent object"
                );
            }
            AssignNestedInt(
                current_frame_data,
                nested_field,
                key,
                ParseInt(value, yaml_path, line_number));
            continue;
        }
    }

    if (have_current_frame_data) {
        result.sprites.push_back(current_frame_data);
    }

    return result;
}

} // namespace splonks

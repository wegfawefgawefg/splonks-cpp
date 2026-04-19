#include "raw_audio_asset.hpp"

#include <cctype>
#include <fstream>
#include <stdexcept>
#include <string>

namespace splonks {

namespace {

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

std::string ParseString(const std::string& value) {
    return Trim(StripQuotes(value));
}

float ParseFloat(const std::string& value, const std::string& yaml_path, int line_number) {
    try {
        std::size_t parsed = 0;
        const float result = std::stof(value, &parsed);
        if (parsed != value.size()) {
            throw std::runtime_error("");
        }
        return result;
    } catch (...) {
        throw std::runtime_error(
            yaml_path + ":" + std::to_string(line_number) + ": invalid float: " + value
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

void AssignAudioField(
    RawAudioAsset& asset,
    const std::string& key,
    const std::string& value,
    const std::string& yaml_path,
    int line_number
) {
    if (key == "name") {
        asset.name = ParseString(value);
    } else if (key == "file") {
        asset.file = ParseString(value);
    } else if (key == "default_volume") {
        asset.default_volume = ParseFloat(value, yaml_path, line_number);
    } else if (key == "streamed") {
        asset.streamed = ParseBool(value, yaml_path, line_number);
    }
}

} // namespace

RawAudioAssetFile LoadRawAudioAssetFile(const std::string& yaml_path) {
    std::ifstream file(yaml_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open audio annotations file: " + yaml_path);
    }

    RawAudioAssetFile result;
    RawAudioAsset current_asset;
    bool in_audio = false;
    bool have_current_asset = false;

    std::string line;
    int line_number = 0;
    while (std::getline(file, line)) {
        line_number += 1;
        const std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed.front() == '#') {
            continue;
        }

        const int indent = CountIndent(line);
        if (trimmed == "audio:") {
            in_audio = true;
            continue;
        }
        if (!in_audio) {
            continue;
        }

        if (indent == 2 && trimmed.rfind("- ", 0) == 0) {
            if (have_current_asset) {
                result.audio.push_back(current_asset);
            }
            current_asset = RawAudioAsset{};
            current_asset.source_yaml_path = yaml_path;
            current_asset.source_line = line_number;
            have_current_asset = true;

            const std::string remainder = Trim(trimmed.substr(2));
            if (!remainder.empty()) {
                const auto [key, value] = SplitKeyValue(remainder, yaml_path, line_number);
                AssignAudioField(current_asset, key, value, yaml_path, line_number);
            }
            continue;
        }

        if (!have_current_asset) {
            throw std::runtime_error(
                yaml_path + ":" + std::to_string(line_number) +
                ": audio field encountered before item"
            );
        }

        if (indent == 4) {
            const auto [key, value] = SplitKeyValue(trimmed, yaml_path, line_number);
            AssignAudioField(current_asset, key, value, yaml_path, line_number);
            continue;
        }
    }

    if (have_current_asset) {
        result.audio.push_back(current_asset);
    }

    return result;
}

} // namespace splonks

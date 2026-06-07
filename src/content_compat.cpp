#include "content_compat.hpp"

#include "quest.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace splonks {

namespace {

constexpr std::uint64_t kFnvOffset = 14695981039346656037ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

void HashByte(std::uint64_t& hash, std::uint8_t value) {
    hash ^= static_cast<std::uint64_t>(value);
    hash *= kFnvPrime;
}

void HashU64(std::uint64_t& hash, std::uint64_t value) {
    for (unsigned int shift = 0; shift < 64; shift += 8) {
        HashByte(hash, static_cast<std::uint8_t>((value >> shift) & 0xFFULL));
    }
}

void HashString(std::uint64_t& hash, const std::string& value) {
    HashU64(hash, static_cast<std::uint64_t>(value.size()));
    for (char c : value) {
        HashByte(hash, static_cast<std::uint8_t>(static_cast<unsigned char>(c)));
    }
}

void HashFileBytes(std::uint64_t& hash, const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open gameplay content file: " + path.string());
    }

    HashU64(hash, std::filesystem::file_size(path));
    char byte = 0;
    while (file.get(byte)) {
        HashByte(hash, static_cast<std::uint8_t>(static_cast<unsigned char>(byte)));
    }
}

} // namespace

std::uint64_t ComputeGameplayContentHash() {
    static std::optional<std::uint64_t> cached_hash;
    if (cached_hash.has_value()) {
        return *cached_hash;
    }

    const std::filesystem::path root(GetClassicQuestRootPath());
    if (!std::filesystem::exists(root)) {
        throw std::runtime_error("Gameplay content root is missing: " + root.string());
    }

    std::vector<std::filesystem::path> paths;
    for (const std::filesystem::directory_entry& entry :
         std::filesystem::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        paths.push_back(entry.path());
    }

    std::sort(paths.begin(), paths.end(), [&](const auto& left, const auto& right) {
        return left.lexically_relative(root).generic_string() <
               right.lexically_relative(root).generic_string();
    });

    std::uint64_t hash = kFnvOffset;
    HashString(hash, "splonks-gameplay-content-v1");
    HashU64(hash, static_cast<std::uint64_t>(paths.size()));
    for (const std::filesystem::path& path : paths) {
        HashString(hash, path.lexically_relative(root).generic_string());
        HashFileBytes(hash, path);
    }
    cached_hash = hash;
    return *cached_hash;
}

} // namespace splonks

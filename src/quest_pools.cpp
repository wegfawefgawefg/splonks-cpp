#include "quest.hpp"

#include "quest_parse_utils.hpp"

#include <stdexcept>

namespace splonks {

const EntPoolConfig* ItemPoolDb::FindPool(std::string_view id) const {
    const auto it = pools.find(std::string(id));
    return it == pools.end() ? nullptr : &it->second;
}

const ShopTypeConfig* ShopConfigDb::FindShopType(std::string_view id) const {
    const auto it = shop_types.find(std::string(id));
    return it == shop_types.end() ? nullptr : &it->second;
}

ItemPoolDb LoadItemPoolDb(const std::string& quest_root_path, const std::string& pool_file_path) {
    using namespace quest_parse;

    const std::string path = ResolveQuestPath(quest_root_path, pool_file_path).string();
    const std::vector<std::string> lines = ReadLines(path);
    ItemPoolDb db;
    EntPoolConfig current_pool;
    WeightedEntEntry* current_entry = nullptr;
    bool has_pool = false;
    bool in_pools = false;
    bool in_entries = false;

    const auto finish_pool = [&]() {
        if (!has_pool) {
            return;
        }
        if (current_pool.id.empty()) {
            throw std::runtime_error(path + ": ent pool missing id");
        }
        if (current_pool.entries.empty()) {
            throw std::runtime_error(path + ": ent pool has no entries: " + current_pool.id);
        }
        db.pools[current_pool.id] = current_pool;
        current_pool = EntPoolConfig{};
        current_entry = nullptr;
        has_pool = false;
        in_entries = false;
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
            if (trimmed == "pools:") {
                in_pools = true;
                continue;
            }
            throw std::runtime_error(path + ":" + std::to_string(line_number) +
                                     ": unknown item pool file field");
        }
        if (!in_pools) {
            throw std::runtime_error(path + ":" + std::to_string(line_number) +
                                     ": pool entry before pools block");
        }
        if (indent == 2 && trimmed.rfind("- ", 0) == 0) {
            finish_pool();
            has_pool = true;
            const auto [key, value] = SplitKeyValue(trimmed.substr(2), path, line_number);
            if (key != "id") {
                throw std::runtime_error(path + ":" + std::to_string(line_number) +
                                         ": pool entry must start with id");
            }
            current_pool.id = value;
            continue;
        }
        if (!has_pool) {
            throw std::runtime_error(path + ":" + std::to_string(line_number) +
                                     ": pool field before id");
        }
        if (indent == 4) {
            current_entry = nullptr;
            const auto [key, value] = SplitKeyValue(trimmed, path, line_number);
            if (key == "unique") {
                current_pool.unique = ParseBool(value, path, line_number);
            } else if (key == "entries") {
                if (!value.empty()) {
                    throw std::runtime_error(path + ":" + std::to_string(line_number) +
                                             ": entries must be a block");
                }
                in_entries = true;
            } else {
                throw std::runtime_error(path + ":" + std::to_string(line_number) +
                                         ": unknown pool field: " + key);
            }
            continue;
        }
        if (in_entries && indent == 6 && trimmed.rfind("- ", 0) == 0) {
            const auto [key, value] = SplitKeyValue(trimmed.substr(2), path, line_number);
            if (key != "ent") {
                throw std::runtime_error(path + ":" + std::to_string(line_number) +
                                         ": pool entry must start with ent");
            }
            current_pool.entries.push_back(WeightedEntEntry{
                .ent_type = ParseEntTypeOrThrow(value, path, line_number),
                .weight = 1,
            });
            current_entry = &current_pool.entries.back();
            continue;
        }
        if (in_entries && indent == 8 && current_entry != nullptr) {
            const auto [key, value] = SplitKeyValue(trimmed, path, line_number);
            if (key == "weight") {
                current_entry->weight = ParseInt(value, path, line_number);
            } else if (key == "unique") {
                current_entry->unique = ParseBool(value, path, line_number);
            } else {
                throw std::runtime_error(path + ":" + std::to_string(line_number) +
                                         ": unknown pool entry field: " + key);
            }
            continue;
        }
        throw std::runtime_error(path + ":" + std::to_string(line_number) +
                                 ": unsupported item pool YAML shape");
    }
    finish_pool();
    return db;
}

ShopConfigDb LoadShopConfigDb(
    const std::string& quest_root_path,
    const std::string& shop_file_path
) {
    using namespace quest_parse;

    const std::string path = ResolveQuestPath(quest_root_path, shop_file_path).string();
    const std::vector<std::string> lines = ReadLines(path);
    ShopConfigDb db;
    ShopTypeConfig current_shop;
    bool has_shop = false;
    bool in_shop_types = false;

    const auto finish_shop = [&]() {
        if (!has_shop) {
            return;
        }
        if (current_shop.id.empty()) {
            throw std::runtime_error(path + ": shop type missing id");
        }
        db.shop_types[current_shop.id] = current_shop;
        current_shop = ShopTypeConfig{};
        has_shop = false;
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
            if (trimmed == "shop_types:") {
                in_shop_types = true;
                continue;
            }
            throw std::runtime_error(path + ":" + std::to_string(line_number) +
                                     ": unknown shop file field");
        }
        if (!in_shop_types) {
            throw std::runtime_error(path + ":" + std::to_string(line_number) +
                                     ": shop entry before shop_types block");
        }
        if (indent == 2 && trimmed.rfind("- ", 0) == 0) {
            finish_shop();
            has_shop = true;
            const auto [key, value] = SplitKeyValue(trimmed.substr(2), path, line_number);
            if (key != "id") {
                throw std::runtime_error(path + ":" + std::to_string(line_number) +
                                         ": shop type entry must start with id");
            }
            current_shop.id = value;
            continue;
        }
        if (!has_shop || indent != 4) {
            throw std::runtime_error(path + ":" + std::to_string(line_number) +
                                     ": unsupported shop YAML shape");
        }
        const auto [key, value] = SplitKeyValue(trimmed, path, line_number);
        if (key == "sign") {
            current_shop.sign = ParseEntTypeOrThrow(value, path, line_number);
        } else if (key == "item_pool") {
            current_shop.item_pool = value;
        } else if (key == "item_slots") {
            current_shop.item_slots = ParseInt(value, path, line_number);
        } else {
            throw std::runtime_error(path + ":" + std::to_string(line_number) +
                                     ": unknown shop field: " + key);
        }
    }
    finish_shop();
    return db;
}

} // namespace splonks

#pragma once

#include <cstdint>

namespace splonks {

struct VID {
    std::uint32_t id = 0;
    std::uint32_t version = 0;
};

inline bool operator==(const VID& left, const VID& right) {
    return left.id == right.id && left.version == right.version;
}

} // namespace splonks

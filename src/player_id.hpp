#pragma once

#include <cstdint>

namespace splonks {

using PlayerId = std::uint32_t;

constexpr PlayerId kInvalidPlayerId = 0;
constexpr PlayerId kPrimaryLocalPlayerId = 1;

} // namespace splonks

#pragma once

#include <cstddef>

namespace splonks::network {

// Keep packets comfortably below normal UDP MTU while allowing one complete broad entity-state patch.
constexpr std::size_t kNetPacketMaxBytes = 576;

} // namespace splonks::network

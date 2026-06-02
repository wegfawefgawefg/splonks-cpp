#pragma once

#include <cstddef>
#include <cstdint>

namespace splonks::network {

// Keep packets comfortably below normal UDP MTU while allowing one complete broad ent-state patch.
constexpr std::size_t kNetPacketMaxBytes = 576;
constexpr std::size_t kNetTransportDatagramMaxBytes = 1400;
constexpr std::uint16_t kDefaultMultiplayerPort = 39000;

} // namespace splonks::network

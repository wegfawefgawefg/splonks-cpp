#pragma once

#include <cstddef>

namespace splonks {

struct Audio;
struct Graphics;
struct State;

namespace network {

struct NetSessionState;

std::size_t ApplyOrderedMessages(
    NetSessionState& session,
    State& state,
    Audio* audio = nullptr,
    Graphics* graphics = nullptr
);

} // namespace network
} // namespace splonks

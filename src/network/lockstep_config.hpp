#pragma once

#include <cstdint>

namespace splonks::network {

constexpr std::uint32_t kDefaultLockstepInputDelayFrames = 8;
constexpr std::uint32_t kMinLockstepInputDelayFrames = 1;
constexpr std::uint32_t kMaxLockstepInputDelayFrames = 60;

constexpr std::uint32_t ClampLockstepInputDelayFrames(std::uint32_t frames) {
    if (frames < kMinLockstepInputDelayFrames) {
        return kMinLockstepInputDelayFrames;
    }
    if (frames > kMaxLockstepInputDelayFrames) {
        return kMaxLockstepInputDelayFrames;
    }
    return frames;
}

} // namespace splonks::network

#pragma once

#include <cstdint>

namespace splonks::network {

constexpr std::uint32_t kDefaultLockstepInputDelayFrames = 2;
constexpr std::uint32_t kMinLockstepInputDelayFrames = 1;
constexpr std::uint32_t kMaxLockstepInputDelayFrames = 60;
constexpr std::uint32_t kDefaultLockstepMaxRollbackFrames = 12;
constexpr std::uint32_t kMinLockstepMaxRollbackFrames = 2;
constexpr std::uint32_t kMaxLockstepMaxRollbackFrames = 120;
constexpr std::uint32_t kLockstepSettingsApplySafetyFrames = 4;
constexpr std::uint32_t kLockstepAutoDelayStableFrames = 180;

constexpr std::uint32_t ClampLockstepInputDelayFrames(std::uint32_t frames) {
    if (frames < kMinLockstepInputDelayFrames) {
        return kMinLockstepInputDelayFrames;
    }
    if (frames > kMaxLockstepInputDelayFrames) {
        return kMaxLockstepInputDelayFrames;
    }
    return frames;
}

constexpr std::uint32_t ClampLockstepMaxRollbackFrames(std::uint32_t frames) {
    if (frames < kMinLockstepMaxRollbackFrames) {
        return kMinLockstepMaxRollbackFrames;
    }
    if (frames > kMaxLockstepMaxRollbackFrames) {
        return kMaxLockstepMaxRollbackFrames;
    }
    return frames;
}

} // namespace splonks::network

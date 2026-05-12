#pragma once

#include "player_id.hpp"

#include <cstdint>
#include <string>

namespace splonks {

struct State;

struct CanonicalStateFingerprint {
    std::uint64_t value = 0;
    std::string summary;
};

CanonicalStateFingerprint ComputeCanonicalStateFingerprint(const State& state);
CanonicalStateFingerprint ComputeGameplayDeterminismFingerprint(const State& state);
CanonicalStateFingerprint ComputeNetworkStateFingerprint(const State& state);
CanonicalStateFingerprint ComputeNetworkStateFingerprintIgnoringPlayerMotion(
    const State& state,
    PlayerId player_id
);

} // namespace splonks

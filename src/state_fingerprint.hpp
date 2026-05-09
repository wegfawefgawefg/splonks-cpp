#pragma once

#include <cstdint>
#include <string>

namespace splonks {

struct State;

struct CanonicalStateFingerprint {
    std::uint64_t value = 0;
    std::string summary;
};

CanonicalStateFingerprint ComputeCanonicalStateFingerprint(const State& state);
CanonicalStateFingerprint ComputeNetworkStateFingerprint(const State& state);

} // namespace splonks

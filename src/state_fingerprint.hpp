#pragma once

#include "player_id.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace splonks {

struct State;

struct CanonicalStateFingerprint {
    std::uint64_t value = 0;
    std::string summary;
};

struct NetworkStateFingerprintComponents {
    std::uint64_t root = 0;
    std::uint64_t stage = 0;
    std::uint64_t players = 0;
    std::uint64_t tools = 0;
    std::uint64_t ents = 0;
};

struct NetworkEntFingerprint {
    std::uint64_t net_ent_id = 0;
    std::uint16_t type = 0;
    std::uint64_t hash = 0;
};

CanonicalStateFingerprint ComputeCanonicalStateFingerprint(const State& state);
CanonicalStateFingerprint ComputeGameplayDeterminismFingerprint(const State& state);
CanonicalStateFingerprint ComputeNetworkStateFingerprint(const State& state);
NetworkStateFingerprintComponents ComputeNetworkStateFingerprintComponents(const State& state);
std::vector<NetworkEntFingerprint> ComputeNetworkEntFingerprints(const State& state);

} // namespace splonks

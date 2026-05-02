#pragma once

#include <cstdint>

namespace splonks {

enum class ParticleLightingMode : std::uint8_t {
    SceneLit,
    Unlit,
    Emissive,
};

} // namespace splonks


#pragma once

#include <cstdint>

namespace splonks {

enum class EffectId : std::uint8_t {
    None,
    Gloves,
    Spectacles,
    Compass,
    Mitt,
    SpringShoes,
    SpikeShoes,
    UdjatEye,
    Ankh,
    Meathead,
    Parachute,
    NoGravityUntilContact,
    InWater,
    Count,
};

} // namespace splonks

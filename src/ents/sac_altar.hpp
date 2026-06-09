#pragma once

#include "ent/spec.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace splonks {
struct Graphics;
struct State;
struct Audio;
struct Ent;
}

namespace splonks::ents::sac_altar {

extern const EntSpec kSacAltarSpec;

void OnDeathAsSacAltarPiece(std::size_t ent_idx, State& state, Audio& audio);
std::optional<std::int32_t> GetSacrificeFavorValue(const Ent& victim);
std::optional<std::int32_t> GetLivingSacrificeFavorValue(const Ent& victim);
void SpawnSacrificeGainEffects(State& state, Audio& audio, const FVec2& pos);
bool TryDepositStoredFavor(
    Ent& altar_piece,
    std::int32_t favor,
    State& state,
    const Graphics& graphics,
    Audio& audio
);

} // namespace splonks::ents::sac_altar

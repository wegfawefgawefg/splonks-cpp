#pragma once

#include "entity/archetype.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace splonks {
struct Graphics;
struct State;
struct Audio;
struct Entity;
}

namespace splonks::entities::sac_altar {

extern const EntityArchetype kSacAltarArchetype;

void OnDeathAsSacAltarPiece(std::size_t entity_idx, State& state, Audio& audio);
std::optional<std::int32_t> GetSacrificeFavorValue(const Entity& victim);
std::optional<std::int32_t> GetLivingSacrificeFavorValue(const Entity& victim);
void SpawnSacrificeGainEffects(State& state, Audio& audio, const Vec2& pos);
bool TryDepositStoredFavor(
    Entity& altar_piece,
    std::int32_t favor,
    State& state,
    const Graphics& graphics,
    Audio& audio
);

} // namespace splonks::entities::sac_altar

#pragma once

#include "ent.hpp"

#include <optional>

namespace splonks {

struct Audio;
struct Graphics;
struct State;

void ClearEntBuyableState(Ent& ent);
void ConfigureEntAsBuyable(Ent& ent, std::uint32_t price);
bool TrySpendMoney(std::size_t buyer_idx, std::uint32_t amount, State& state, Audio& audio);
bool TryBuyEntForMoney(
    std::size_t ent_idx,
    std::size_t buyer_idx,
    State& state,
    Graphics& graphics,
    Audio& audio
);
void AddBuyPromptsForPlayer(State& state, const Graphics& graphics);
std::optional<std::size_t> FindOverlappingBuyableEntIdx(
    const State& state,
    const Graphics& graphics,
    std::size_t buyer_idx
);
bool TryBuyEnt(
    std::size_t ent_idx,
    std::size_t buyer_idx,
    State& state,
    Graphics& graphics,
    Audio& audio
);

} // namespace splonks

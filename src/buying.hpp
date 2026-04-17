#pragma once

#include "entity.hpp"

#include <optional>

namespace splonks {

struct Audio;
struct Graphics;
struct State;

struct BuyPrompt {
    std::size_t entity_idx = 0;
    std::uint32_t quantity = 0;
    std::optional<FrameDataId> icon_animation_id = std::nullopt;
};

void ClearEntityBuyableState(Entity& entity);
bool TrySpendMoney(std::size_t buyer_idx, std::uint32_t amount, State& state, Audio& audio);
bool TryBuyEntityForMoney(
    std::size_t entity_idx,
    std::size_t buyer_idx,
    State& state,
    Graphics& graphics,
    Audio& audio
);
std::optional<BuyPrompt> GetBuyPromptForPlayer(const State& state, const Graphics& graphics);
bool TryBuyOverlappingEntity(std::size_t buyer_idx, State& state, Graphics& graphics, Audio& audio);
bool TryBuyEntity(
    std::size_t entity_idx,
    std::size_t buyer_idx,
    State& state,
    Graphics& graphics,
    Audio& audio
);

} // namespace splonks

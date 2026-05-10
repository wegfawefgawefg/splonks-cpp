#pragma once

#include "network/net_event.hpp"

namespace splonks {

struct Audio;
struct State;

namespace network {

void ApplyTileBrokenEvent(State& state, Audio* audio, const TileBrokenEvent& payload);
void ApplyTileChangedEvent(State& state, const TileChangedEvent& payload);
void ApplyFluidCellPatchedEvent(State& state, const FluidCellPatchedEvent& payload);

} // namespace network
} // namespace splonks

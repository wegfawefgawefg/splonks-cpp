#pragma once

#include "stage.hpp"
#include "state.hpp"

namespace splonks {

Stage MakeShopTestStage();
void InitShopTestStage(State& state);
void SpawnShopTestStoreLight(State& state, int anchor_tile_x, int start_tile_y);

} // namespace splonks

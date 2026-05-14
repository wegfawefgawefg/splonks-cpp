#pragma once

#include "stage.hpp"
#include "stage_gen/classic/item_pools.hpp"

namespace splonks::stage_gen::classic {

void ConvertExitTilesToBasicExitSpawns(Stage& stage);
void AddBranchExit(Stage& stage, const StagePassConfig& pass, DetRng& det_rng);
bool AddUdjatKeyChest(Stage& stage, DetRng& det_rng);
void AddMinesEmbeddedTreasure(Stage& stage, const ItemPoolDb& item_db, DetRng& det_rng);
void AddMinesTreasure(Stage& stage, int level_number, DetRng& det_rng);
void ConvertBlocksToArrowTraps(Stage& stage, int chance_denominator, DetRng& det_rng);

} // namespace splonks::stage_gen::classic

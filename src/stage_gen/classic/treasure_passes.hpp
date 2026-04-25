#pragma once

#include "stage.hpp"
#include "stage_gen/classic/item_pools.hpp"

namespace splonks::stage_gen::classic {

void ConvertExitTilesToBasicExitSpawns(Stage& stage);
void AddBranchExit(Stage& stage, const StagePassConfig& pass);
void AddUdjatKeyChest(Stage& stage);
void AddMinesEmbeddedTreasure(Stage& stage, const ItemPoolDb& item_db);
void AddMinesTreasure(Stage& stage, int level_number);
void ConvertBlocksToArrowTraps(Stage& stage);

} // namespace splonks::stage_gen::classic

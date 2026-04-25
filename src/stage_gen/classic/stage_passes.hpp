#pragma once

#include "quest.hpp"
#include "stage.hpp"
#include "stage_gen/classic/item_pools.hpp"

#include <string>

namespace splonks::stage_gen::classic {

void AddStageGenAnnotation(Stage& stage, const std::string& text);
void RunStagePass(Stage& stage, int level_number, const StagePassConfig& pass,
                  const ItemPoolDb& item_db);

} // namespace splonks::stage_gen::classic

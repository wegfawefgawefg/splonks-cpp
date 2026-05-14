#pragma once

#include "quest.hpp"
#include "stage.hpp"
#include "stage_gen/classic/item_pools.hpp"

#include <string>

namespace splonks::stage_gen::classic {

void AddStageGenAnnotation(Stage& stage, const std::string& text);
void RunStagePass(Stage& stage, int level_number, const StagePassConfig& pass,
                  const ItemPoolDb& item_db, DetRng& det_rng,
                  QuestState* quest_state = nullptr);

} // namespace splonks::stage_gen::classic

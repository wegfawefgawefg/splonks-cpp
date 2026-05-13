#pragma once

#include "stage.hpp"

namespace splonks::stage_gen::classic {

void AddAmbientMinesEnts(Stage& stage);
void AddAmbientJungleEnts(Stage& stage, bool black_market);
void AddAmbientIceEnts(Stage& stage);
void AddAmbientTempleEnts(Stage& stage);
void AddAmbientOlmecEnts(Stage& stage);

} // namespace splonks::stage_gen::classic

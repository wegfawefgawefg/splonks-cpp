#pragma once

#include "stage.hpp"

namespace splonks::stage_gen::classic {

void AddAmbientMinesEntities(Stage& stage);
void AddAmbientJungleEntities(Stage& stage, bool black_market);
void AddAmbientIceEntities(Stage& stage);
void AddAmbientTempleEntities(Stage& stage);
void AddAmbientOlmecEntities(Stage& stage);

} // namespace splonks::stage_gen::classic

#pragma once

#include "stage.hpp"

namespace splonks::stage_gen::classic {

void AddAmbientMinesEnts(Stage& stage, DetRng& det_rng);
void AddAmbientJungleEnts(Stage& stage, bool black_market, DetRng& det_rng);
void AddAmbientIceEnts(Stage& stage, DetRng& det_rng);
void AddAmbientTempleEnts(Stage& stage, DetRng& det_rng);
void AddAmbientOlmecEnts(Stage& stage);

} // namespace splonks::stage_gen::classic

#pragma once

#include "entity.hpp"

namespace splonks {

struct Audio;
struct State;

}

namespace splonks::entities::giant_tiki_head {

void SetEntityGiantTikiHead(Entity& entity);
void StepEntityLogicAsGiantTikiHead(std::size_t entity_idx, State& state, Audio& audio);

} // namespace splonks::entities::giant_tiki_head

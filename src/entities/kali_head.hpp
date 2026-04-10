#pragma once

#include "entity.hpp"

namespace splonks {

struct Audio;
struct State;

}

namespace splonks::entities::kali_head {

void SetEntityKaliHead(Entity& entity);
void StepEntityLogicAsKaliHead(std::size_t entity_idx, State& state, Audio& audio);

} // namespace splonks::entities::kali_head

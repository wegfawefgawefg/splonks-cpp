#pragma once

#include "state.hpp"

namespace splonks {

void OnDamageEffectAsBreakawayContainer(std::size_t entity_idx, State& state);
void OnDamageEffectAsBleedingEntity(std::size_t entity_idx, State& state);

} // namespace splonks

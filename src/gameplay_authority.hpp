#pragma once

#include "vid.hpp"

namespace splonks {

struct State;

bool HasLocalGameplayAuthorityForEntity(const State& state, VID entity_vid);
bool HasLocalGameplayAuthorityForInteractionSource(const State& state, VID entity_vid);

} // namespace splonks

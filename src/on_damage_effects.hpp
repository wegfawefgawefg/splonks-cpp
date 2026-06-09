#pragma once

#include "aframe_id.hpp"
#include "state.hpp"

namespace splonks {

void SpawnDamageEffectAnimBurst(AFrameId anim_id, const FVec2& center, State& state);
void SpawnBreakawayContainerShards(const FVec2& center, State& state);

} // namespace splonks

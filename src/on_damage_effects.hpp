#pragma once

#include "frame_data_id.hpp"
#include "state.hpp"

namespace splonks {

void SpawnDamageEffectAnimationBurst(FrameDataId animation_id, const Vec2& center, State& state);
void SpawnBreakawayContainerShards(const Vec2& center, State& state);

} // namespace splonks

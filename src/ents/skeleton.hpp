#pragma once

#include "ent/spec.hpp"

namespace splonks {

struct Audio;
struct Graphics;
struct State;

} // namespace splonks

namespace splonks::ents::common {
struct ContactContext;
}

namespace splonks::ents::skeleton {

extern const EntSpec kSkullSpec;
extern const EntSpec kSkeletonSpec;

void StepEntLogicAsSkeleton(
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    float dt
);

void OnDeathAsSkull(std::size_t ent_idx, State& state, Audio& audio);
void OnDeathAsSkeleton(std::size_t ent_idx, State& state, Audio& audio);

bool TryApplySkullTileImpact(
    std::size_t ent_idx,
    const common::ContactContext& context,
    State& state
);

bool TryApplySkullEntImpact(
    std::size_t ent_idx,
    std::size_t other_ent_idx,
    const common::ContactContext& context,
    State& state
);

} // namespace splonks::ents::skeleton

#include "network/net_message_apply_internal.hpp"

#include "stage_lighting.hpp"
#include "state.hpp"

#include <cstddef>

namespace splonks::network {

void ApplyStageLightAddedMessage(State& state, const StageLightAddedMessage& payload) {
    if (payload.radius <= 0) {
        return;
    }
    AddStageLightWithVid(
        state,
        VID{static_cast<std::size_t>(payload.light_id)},
        payload.tile_pos,
        payload.radius
    );
}

void ApplyStageLightRemovedMessage(State& state, const StageLightRemovedMessage& payload) {
    (void)RemoveStageLight(state, VID{static_cast<std::size_t>(payload.light_id)});
}

} // namespace splonks::network

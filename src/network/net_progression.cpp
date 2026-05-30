#include "network/net_progression.hpp"

#include "network/net_ent_links.hpp"
#include "network/net_lobby_internal.hpp"
#include "state.hpp"

namespace splonks::network {

namespace {

std::uint32_t MakeHostStageSeed(const State& state) {
    const std::uint32_t frame_component = state.frame == 0 ? 1U : state.frame;
    return frame_component ^ 0x51A7E5D3U;
}

} // namespace

void NotifyStageLoaded(State& state) {
    if (state.net_session.role == NetRole::Offline || state.stage.quest_id.empty()) {
        return;
    }

    state.net_session.quest_id = state.stage.quest_id;
    state.net_session.quest_stage_id = state.stage.quest_stage_id;
    state.net_session.stage_seed = state.stage.generation_seed.value_or(MakeHostStageSeed(state));
    state.net_session.stage_instance_id += 1;
    ResetInputLockstepState(state);
    NotifyRunRestartStageLoaded(state);
    state.net_session.ClearStageEntLinks();
    RegisterStageEntLinks(state);
}

} // namespace splonks::network

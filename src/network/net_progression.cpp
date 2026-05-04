#include "network/net_progression.hpp"

#include "graphics.hpp"
#include "network/net_entity_links.hpp"
#include "quest_stage_loader.hpp"
#include "stage_progression.hpp"
#include "state.hpp"

namespace splonks::network {

namespace {

std::uint32_t MakeHostStageSeed(const State& state) {
    const std::uint32_t frame_component = state.frame == 0 ? 1U : state.frame;
    return frame_component ^ 0x51A7E5D3U;
}

bool LoadNetworkQuestStage(
    State& state,
    const std::string& quest_id,
    const std::string& quest_stage_id,
    std::uint32_t seed,
    bool preserve_player_state
) {
    return LoadQuestStage(
        state,
        quest_id,
        quest_stage_id,
        preserve_player_state,
        seed
    );
}

void SendEncodedPacket(
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint,
    const EncodedNetPacket& encoded
) {
    std::string error;
    if (!transport.socket.Send(endpoint, encoded.bytes.data(), encoded.size, &error)) {
        transport.last_error = error;
    }
}

StageSyncPacket MakeStageSyncPacket(const State& state) {
    StageSyncPacket packet;
    packet.stage_instance_id = state.net_session.stage_instance_id;
    packet.stage_seed = state.net_session.stage_seed;
    WriteFixedString(state.net_session.quest_id, packet.quest_id);
    WriteFixedString(state.net_session.quest_stage_id, packet.quest_stage_id);
    return packet;
}

} // namespace

void SendStageSyncToAllRemotes(State& state, NetTransportRuntime& transport) {
    if (state.net_session.role != NetRole::Coordinator || transport.remotes.empty()) {
        return;
    }

    const EncodedNetPacket encoded = EncodeStageSync(MakeStageSyncPacket(state));
    for (const NetRemoteEndpoint& remote : transport.remotes) {
        SendEncodedPacket(transport, remote.endpoint, encoded);
    }
}

void ApplyStageSync(
    State& state,
    const Graphics& graphics,
    NetTransportRuntime& transport,
    const StageSyncPacket& packet
) {
    if (packet.stage_instance_id == kInvalidStageInstanceId ||
        packet.stage_instance_id == state.net_session.stage_instance_id) {
        return;
    }

    const std::string quest_id = ReadFixedString(packet.quest_id);
    const std::string quest_stage_id = ReadFixedString(packet.quest_stage_id);
    if (quest_id.empty() || quest_stage_id.empty()) {
        return;
    }

    state.net_session.stage_instance_id = packet.stage_instance_id;
    state.net_session.quest_id = quest_id;
    state.net_session.quest_stage_id = quest_stage_id;
    state.net_session.stage_seed = packet.stage_seed;
    state.net_session.ClearStageEntityLinks();
    state.net_session.ordered_events.clear();
    state.net_session.pending_local_events.clear();
    state.net_session.applied_event_ids.clear();
    transport.remote_player_targets.clear();

    if (!LoadNetworkQuestStage(state, quest_id, quest_stage_id, packet.stage_seed, true)) {
        transport.last_error = "Stage sync failed: could not load " + quest_stage_id + ".";
        return;
    }
    RegisterStageEntityLinks(state);

    state.pending_stage_transition.reset();
    state.frame = 0;
    state.scene_frame = 0;
    state.SetMode(Mode::Playing);
    for (const PlayerSlot& slot : state.players.slots) {
        if (slot.entity_vid.has_value()) {
            state.UpdateSidForEntity(slot.entity_vid->id, graphics);
        }
    }
}

void HandleStageExitRequestAsCoordinator(State& state, const StageExitRequestPacket& request) {
    if (request.stage_instance_id != state.net_session.stage_instance_id ||
        state.pending_stage_transition.has_value()) {
        return;
    }

    const StageExitId exit_id = static_cast<StageExitId>(request.exit_id);
    if (!IsStageExitAllowed(state, exit_id)) {
        return;
    }

    QueueStageExitTransition(state, exit_id);
}

void RequestStageExit(State& state, StageExitId exit_id) {
    if (state.net_session.role != NetRole::Peer ||
        !state.net_transport ||
        !state.net_transport->socket.IsOpen()) {
        return;
    }

    StageExitRequestPacket request;
    request.stage_instance_id = state.net_session.stage_instance_id;
    request.player_id = state.net_session.local_player_id;
    request.exit_id = static_cast<std::int32_t>(exit_id);
    SendEncodedPacket(
        *state.net_transport,
        state.net_transport->coordinator_endpoint,
        EncodeStageExitRequest(request)
    );
}

void NotifyStageLoaded(State& state) {
    if (state.net_session.role != NetRole::Coordinator || state.stage.quest_id.empty()) {
        return;
    }

    state.net_session.quest_id = state.stage.quest_id;
    state.net_session.quest_stage_id = state.stage.quest_stage_id;
    state.net_session.stage_seed = state.stage.generation_seed.value_or(MakeHostStageSeed(state));
    state.net_session.stage_instance_id += 1;
    state.net_session.ClearStageEntityLinks();
    state.net_session.ordered_events.clear();
    state.net_session.pending_local_events.clear();
    state.net_session.applied_event_ids.clear();
    RegisterStageEntityLinks(state);
    if (state.net_transport) {
        state.net_transport->remote_player_targets.clear();
        SendStageSyncToAllRemotes(state, *state.net_transport);
    }
}

} // namespace splonks::network

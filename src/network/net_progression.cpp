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

std::uint32_t MakeNetworkTransitionSeed(const State& state) {
    return (state.frame + 1U) ^ (state.stage_frame << 7U) ^ 0x6D2B79F5U;
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

StageSyncPacket MakePendingStageSyncPacket(State& state) {
    StageSyncPacket packet;
    packet.stage_instance_id = state.net_session.stage_instance_id + 1U;
    packet.stage_seed = 1;

    if (!state.pending_stage_transition.has_value() ||
        state.pending_stage_transition->destination.kind != StageLoadTargetKind::QuestStage) {
        return packet;
    }

    StageTransitionTarget& target = *state.pending_stage_transition;
    if (!target.seed.has_value()) {
        target.seed = MakeNetworkTransitionSeed(state);
    }
    packet.stage_seed = *target.seed;
    WriteFixedString(target.destination.quest_id.data(), packet.quest_id);
    WriteFixedString(target.destination.quest_stage_id.data(), packet.quest_stage_id);
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

bool SendPendingStageTransitionSyncToAllRemotes(State& state, NetTransportRuntime& transport) {
    if (state.net_session.role != NetRole::Coordinator ||
        transport.remotes.empty() ||
        !state.pending_stage_transition.has_value() ||
        state.pending_stage_transition->destination.kind != StageLoadTargetKind::QuestStage) {
        return false;
    }

    const StageSyncPacket packet = MakePendingStageSyncPacket(state);
    if (packet.stage_instance_id == kInvalidStageInstanceId ||
        ReadFixedString(packet.quest_id).empty() ||
        ReadFixedString(packet.quest_stage_id).empty()) {
        return false;
    }

    const EncodedNetPacket encoded = EncodeStageSync(packet);
    for (const NetRemoteEndpoint& remote : transport.remotes) {
        SendEncodedPacket(transport, remote.endpoint, encoded);
    }
    return true;
}

void ClearPendingStageSync(NetTransportRuntime& transport) {
    transport.pending_stage_sync = false;
    transport.pending_stage_instance_id = kInvalidStageInstanceId;
    transport.pending_stage_seed = 1;
    transport.pending_quest_id.clear();
    transport.pending_quest_stage_id.clear();
}

void ResetRemoteStageAckState(NetTransportRuntime& transport) {
    for (NetRemoteEndpoint& remote : transport.remotes) {
        remote.highest_acked_coordinator_order = 0;
    }
}

void StorePendingStageSync(
    State& state,
    NetTransportRuntime& transport,
    StageInstanceId stage_instance_id,
    std::uint32_t stage_seed,
    const std::string& quest_id,
    const std::string& quest_stage_id
) {
    if (transport.pending_stage_sync &&
        transport.pending_stage_instance_id == stage_instance_id) {
        return;
    }

    transport.pending_stage_sync = true;
    transport.pending_stage_instance_id = stage_instance_id;
    transport.pending_stage_seed = stage_seed;
    transport.pending_quest_id = quest_id;
    transport.pending_quest_stage_id = quest_stage_id;
    state.pending_stage_transition = StageTransitionTarget{
        .destination = StageLoadTarget::ForQuestStage(quest_id, quest_stage_id),
        .preserve_player_state = true,
        .seed = stage_seed,
    };
    state.frame = 0;
    state.SetMode(Mode::StageTransition);
}

void ApplyStageSyncNow(
    State& state,
    const Graphics& graphics,
    NetTransportRuntime& transport,
    StageInstanceId stage_instance_id,
    std::uint32_t stage_seed,
    const std::string& quest_id,
    const std::string& quest_stage_id
) {
    if (stage_instance_id == kInvalidStageInstanceId ||
        stage_instance_id == state.net_session.stage_instance_id ||
        quest_id.empty() ||
        quest_stage_id.empty()) {
        return;
    }

    state.net_session.stage_instance_id = stage_instance_id;
    state.net_session.quest_id = quest_id;
    state.net_session.quest_stage_id = quest_stage_id;
    state.net_session.stage_seed = stage_seed;
    state.net_session.ClearStageEntityLinks();
    state.net_session.ordered_events.clear();
    state.net_session.pending_outbound_events.clear();
    state.net_session.applied_event_ids.clear();
    transport.remote_player_targets.clear();
    transport.replicated_entity_state_cache.clear();

    if (!LoadNetworkQuestStage(state, quest_id, quest_stage_id, stage_seed, true)) {
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

    if (state.net_session.role == NetRole::Peer) {
        StorePendingStageSync(
            state,
            transport,
            packet.stage_instance_id,
            packet.stage_seed,
            quest_id,
            quest_stage_id
        );
        return;
    }

    ApplyStageSyncNow(
        state,
        graphics,
        transport,
        packet.stage_instance_id,
        packet.stage_seed,
        quest_id,
        quest_stage_id
    );
}

bool ApplyPendingPeerStageSync(State& state, const Graphics& graphics) {
    if (!state.net_transport || !state.net_transport->pending_stage_sync) {
        return false;
    }

    NetTransportRuntime& transport = *state.net_transport;
    const StageInstanceId stage_instance_id = transport.pending_stage_instance_id;
    const std::uint32_t stage_seed = transport.pending_stage_seed;
    const std::string quest_id = transport.pending_quest_id;
    const std::string quest_stage_id = transport.pending_quest_stage_id;
    ClearPendingStageSync(transport);
    ApplyStageSyncNow(
        state,
        graphics,
        transport,
        stage_instance_id,
        stage_seed,
        quest_id,
        quest_stage_id
    );
    return state.net_session.stage_instance_id == stage_instance_id;
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
    state.net_session.pending_outbound_events.clear();
    state.net_session.applied_event_ids.clear();
    RegisterStageEntityLinks(state);
    if (state.net_transport) {
        ClearPendingStageSync(*state.net_transport);
        ResetRemoteStageAckState(*state.net_transport);
        state.net_transport->remote_player_targets.clear();
        state.net_transport->replicated_entity_state_cache.clear();
        SendStageSyncToAllRemotes(state, *state.net_transport);
    }
}

} // namespace splonks::network

#include "network/net_lobby_internal.hpp"

#include "inputs.hpp"
#include "state.hpp"

#include <algorithm>

namespace splonks::network {

namespace {

constexpr LockstepFrame kInputHistoryFrames = 12;

std::vector<PlayerId> GetConnectedPlayerIds(const State& state) {
    std::vector<PlayerId> player_ids;
    for (const PlayerSlot& slot : state.players.slots) {
        if (slot.connected && slot.player_id != kInvalidPlayerId && slot.ent_vid.has_value()) {
            player_ids.push_back(slot.player_id);
        }
    }
    std::sort(player_ids.begin(), player_ids.end());
    return player_ids;
}

std::vector<PlayerId> GetLocalPlayerIds(const State& state) {
    std::vector<PlayerId> player_ids;
    for (const PlayerSlot& slot : state.players.slots) {
        if (slot.connected && slot.connection_kind == PlayerConnectionKind::Local &&
            slot.player_id != kInvalidPlayerId && slot.ent_vid.has_value()) {
            player_ids.push_back(slot.player_id);
        }
    }
    std::sort(player_ids.begin(), player_ids.end());
    return player_ids;
}

InputFrame GetCurrentLocalInputFrame(const State& state, const PlayerSlot& slot) {
    if (slot.primary_local) {
        return ToInputFrame(state.playing_input_snapshot);
    }
    return slot.input_frame;
}

void QueueLocalInputsThroughTargetFrame(State& state) {
    const LockstepFrame target_frame =
        state.net_session.lockstep_next_frame_to_step +
        static_cast<LockstepFrame>(state.net_session.lockstep_input_delay_frames);

    while (state.net_session.lockstep_next_local_input_frame <= target_frame) {
        const LockstepFrame frame = state.net_session.lockstep_next_local_input_frame;
        for (const PlayerSlot& slot : state.players.slots) {
            if (!slot.connected || slot.connection_kind != PlayerConnectionKind::Local ||
                slot.player_id == kInvalidPlayerId || !slot.ent_vid.has_value()) {
                continue;
            }
            LockstepInputRecord record;
            record.player_id = slot.player_id;
            record.frame = frame;
            record.sequence = state.net_session.lockstep_next_input_sequence;
            record.input = GetCurrentLocalInputFrame(state, slot);
            state.net_session.lockstep_input_buffer.Store(record);
        }
        state.net_session.lockstep_next_local_input_frame += 1;
        state.net_session.lockstep_next_input_sequence += 1;
    }
}

InputFrameRecordEntry MakeInputFrameRecordEntry(const LockstepInputRecord& record) {
    InputFrameRecordEntry entry;
    entry.player_id = record.player_id;
    entry.frame = record.frame;
    entry.sequence = record.sequence;
    entry.input_flags = PackInputFrame(record.input);
    entry.mouse_x = record.input.mouse_pos.x;
    entry.mouse_y = record.input.mouse_pos.y;
    return entry;
}

LockstepInputRecord MakeLockstepInputRecord(const InputFrameRecordEntry& entry) {
    LockstepInputRecord record;
    record.player_id = entry.player_id;
    record.frame = entry.frame;
    record.sequence = entry.sequence;
    record.input = UnpackInputFrame(entry.input_flags, UVec2::New(entry.mouse_x, entry.mouse_y));
    return record;
}

InputFrameRecordsPacket BuildLocalInputFramePacket(State& state) {
    InputFrameRecordsPacket packet;
    packet.stage_instance_id = state.net_session.stage_instance_id;
    packet.sender_peer_id = static_cast<std::uint32_t>(state.net_session.local_player_id);

    const std::vector<PlayerId> local_player_ids = GetLocalPlayerIds(state);
    if (local_player_ids.empty() || state.net_session.lockstep_next_local_input_frame == 0) {
        return packet;
    }

    const LockstepFrame last_frame = state.net_session.lockstep_next_local_input_frame - 1;
    const LockstepFrame first_frame = last_frame > kInputHistoryFrames
        ? last_frame - kInputHistoryFrames
        : 0;
    std::vector<LockstepInputRecord> records;
    state.net_session.lockstep_input_buffer.CollectRecords(
        local_player_ids,
        first_frame,
        last_frame,
        records,
        packet.records.size()
    );
    packet.record_count = static_cast<std::uint32_t>(records.size());
    for (std::uint32_t i = 0; i < packet.record_count; ++i) {
        packet.records[i] = MakeInputFrameRecordEntry(records[i]);
    }
    return packet;
}

void SendInputFramePacketToEndpoint(
    State& state,
    NetTransportRuntime& transport,
    const NetEndpoint& endpoint
) {
    const InputFrameRecordsPacket packet = BuildLocalInputFramePacket(state);
    if (packet.record_count == 0) {
        return;
    }
    SendEncodedPacket(transport, endpoint, EncodeInputFrameRecords(packet));
}

void SendLocalInputFramePacket(State& state, NetTransportRuntime& transport) {
    if (state.net_session.role == NetRole::Host) {
        for (const NetRemoteEndpoint& remote : transport.remotes) {
            SendInputFramePacketToEndpoint(state, transport, remote.endpoint);
        }
        return;
    }
    if (state.net_session.role == NetRole::Peer && !transport.join_request_pending) {
        SendInputFramePacketToEndpoint(state, transport, transport.host_endpoint);
    }
}

void ApplyLockstepInputsToState(
    State& state,
    const std::vector<PlayerId>& player_ids,
    const std::vector<InputFrame>& input_frames
) {
    const PlayerSlot* const primary_slot = state.players.FindPrimaryLocal();
    const PlayerId primary_player_id =
        primary_slot != nullptr ? primary_slot->player_id : kInvalidPlayerId;

    for (std::size_t i = 0; i < player_ids.size(); ++i) {
        if (player_ids[i] == primary_player_id) {
            state.playing_input_snapshot = ToPlayingInputSnapshot(input_frames[i]);
        } else {
            state.players.SetInputFrameForPlayer(player_ids[i], input_frames[i]);
        }
    }
}

void PumpInputLockstepPackets(State& state, const Graphics& graphics, NetTransportRuntime& transport) {
    FlushFuzzedOutgoingPackets(transport);
    if (state.net_session.role == NetRole::Host) {
        CleanupExpiredRetainedPlayerStates(state);
        StepHostPackets(state, graphics, transport);
    } else if (state.net_session.role == NetRole::Peer) {
        StepPeerPackets(state, graphics, transport);
    }
    FlushFuzzedOutgoingPackets(transport);
    state.net_session.fuzzer_stats = transport.fuzzer_stats;
}

} // namespace

bool IsInputLockstepActive(const State& state) {
    return IsInputLockstepSession(state) &&
           state.net_transport != nullptr &&
           state.net_transport->socket.IsOpen();
}

bool IsInputLockstepSession(const State& state) {
    return state.net_session.role != NetRole::Offline &&
           state.net_session.input_lockstep_enabled;
}

void ResetInputLockstepState(State& state) {
    state.net_session.lockstep_input_buffer = LockstepInputBuffer{};
    state.net_session.lockstep_next_frame_to_step = 0;
    state.net_session.lockstep_next_local_input_frame = 0;
    state.net_session.lockstep_next_input_sequence = 1;
    state.net_session.lockstep_last_confirmed_hash_frame = 0;
    state.net_session.lockstep_last_confirmed_hash = 0;
}

bool PrepareInputLockstepFrame(State& state, const Graphics& graphics) {
    if (!IsInputLockstepActive(state)) {
        return true;
    }

    NetTransportRuntime& transport = *state.net_transport;
    transport.fuzzer_config = state.net_session.fuzzer_config;
    PumpInputLockstepPackets(state, graphics, transport);
    if (state.net_session.role == NetRole::Host && transport.remotes.empty()) {
        return false;
    }
    if (state.net_session.role == NetRole::Peer && transport.join_request_pending) {
        return false;
    }

    QueueLocalInputsThroughTargetFrame(state);
    SendLocalInputFramePacket(state, transport);
    FlushFuzzedOutgoingPackets(transport);
    state.net_session.fuzzer_stats = transport.fuzzer_stats;

    const std::vector<PlayerId> required_players = GetConnectedPlayerIds(state);
    if (required_players.empty()) {
        return false;
    }

    std::vector<InputFrame> frame_inputs;
    if (!state.net_session.lockstep_input_buffer.BuildFrameInputs(
            required_players,
            state.net_session.lockstep_next_frame_to_step,
            frame_inputs
        )) {
        return false;
    }

    ApplyLockstepInputsToState(state, required_players, frame_inputs);
    state.net_session.lockstep_next_frame_to_step += 1;
    if (state.net_session.lockstep_next_frame_to_step > 4) {
        state.net_session.lockstep_input_buffer.ClearBefore(
            state.net_session.lockstep_next_frame_to_step - 4
        );
    }
    return true;
}

void HandleInputFrameRecords(State& state, const InputFrameRecordsPacket& packet) {
    if (packet.stage_instance_id != state.net_session.stage_instance_id) {
        return;
    }
    for (std::uint32_t i = 0; i < packet.record_count; ++i) {
        state.net_session.lockstep_input_buffer.Store(MakeLockstepInputRecord(packet.records[i]));
    }
}

void RelayInputFrameRecordsToOtherRemotes(
    NetTransportRuntime& transport,
    const NetEndpoint& source_endpoint,
    const InputFrameRecordsPacket& packet
) {
    const EncodedNetPacket encoded = EncodeInputFrameRecords(packet);
    for (const NetRemoteEndpoint& remote : transport.remotes) {
        if (EndpointsEqual(remote.endpoint, source_endpoint)) {
            continue;
        }
        SendEncodedPacket(transport, remote.endpoint, encoded);
    }
}

} // namespace splonks::network

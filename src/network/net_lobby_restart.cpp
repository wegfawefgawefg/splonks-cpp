#include "network/net_lobby.hpp"

#include "network/net_lobby_internal.hpp"
#include "stage_progression.hpp"
#include "state.hpp"

#include <algorithm>
#include <string>

namespace splonks::network {

namespace {

constexpr LockstepFrame kRunRestartSafetyFrames = 30;

std::uint32_t MixRestartSeed(std::uint32_t seed, std::uint32_t value) {
    seed ^= value + 0x9E3779B9U + (seed << 6U) + (seed >> 2U);
    return seed;
}

std::uint32_t MakeRestartSeed(const State& state, LockstepFrame apply_frame) {
    std::uint32_t seed = state.stage.generation_seed.value_or(state.net_session.stage_seed);
    seed = MixRestartSeed(
        seed == 0 ? 1U : seed,
        static_cast<std::uint32_t>(state.net_session.stage_instance_id)
    );
    seed = MixRestartSeed(seed, static_cast<std::uint32_t>(apply_frame));
    seed = MixRestartSeed(seed, static_cast<std::uint32_t>(apply_frame >> 32U));
    return seed == 0 ? 1U : seed;
}

LockstepFrame RestartApplyFrame(const State& state) {
    const LockstepFrame delay =
        std::max<LockstepFrame>(kRunRestartSafetyFrames,
                                state.net_session.lockstep_input_delay_frames * 2ULL);
    return state.net_session.lockstep_next_frame_to_step + delay;
}

void StorePendingRestart(
    State& state,
    std::uint32_t sequence,
    LockstepFrame apply_frame,
    std::uint32_t stage_seed,
    const std::string& quest_id,
    const std::string& quest_stage_id
) {
    state.net_session.run_restart_pending = true;
    state.net_session.run_restart_transition_queued = false;
    state.net_session.run_restart_applied_locally = false;
    state.net_session.run_restart_last_sequence = sequence;
    state.net_session.run_restart_apply_frame = apply_frame;
    state.net_session.run_restart_stage_seed = stage_seed == 0 ? 1U : stage_seed;
    state.net_session.run_restart_quest_id = quest_id;
    state.net_session.run_restart_quest_stage_id = quest_stage_id;
}

RunRestartPacket MakeRunRestartPacket(const State& state) {
    RunRestartPacket packet;
    packet.stage_instance_id = state.net_session.stage_instance_id;
    packet.sender_peer_id = state.net_session.local_player_id;
    packet.sequence = state.net_session.run_restart_last_sequence;
    packet.apply_frame = state.net_session.run_restart_apply_frame;
    packet.stage_seed = state.net_session.run_restart_stage_seed;
    WriteFixedString(state.net_session.run_restart_quest_id, packet.quest_id);
    WriteFixedString(state.net_session.run_restart_quest_stage_id, packet.quest_stage_id);
    return packet;
}

void BroadcastRunRestart(State& state, NetTransportRuntime& transport) {
    if (!state.net_session.run_restart_pending || state.net_session.role != NetRole::Host) {
        return;
    }
    const EncodedNetPacket encoded = EncodeRunRestart(MakeRunRestartPacket(state));
    for (const NetRemoteEndpoint& remote : transport.remotes) {
        SendEncodedPacket(transport, remote.endpoint, encoded);
        state.net_session.run_restart_packets_sent += 1;
    }
}

} // namespace

bool RequestRunRestart(State& state, std::string* status_out) {
    if (!IsInputLockstepActive(state) || state.net_session.role != NetRole::Host ||
        state.net_transport == nullptr) {
        if (status_out != nullptr) {
            *status_out = "Only the host can restart a network run.";
        }
        return false;
    }

    const LockstepFrame apply_frame = RestartApplyFrame(state);
    const std::uint32_t seed = MakeRestartSeed(state, apply_frame);
    const std::uint32_t sequence = state.net_session.run_restart_next_sequence++;
    StorePendingRestart(state, sequence, apply_frame, seed, "classic", "classic_mines_1");
    BroadcastRunRestart(state, *state.net_transport);
    if (status_out != nullptr) {
        *status_out = "Network run restart scheduled.";
    }
    return true;
}

bool RequestRunStart(State& state, std::uint32_t stage_seed, std::string* status_out) {
    if (!IsInputLockstepActive(state) || state.net_session.role != NetRole::Host ||
        state.net_transport == nullptr) {
        if (status_out != nullptr) {
            *status_out = "Only the host can start a network run.";
        }
        return false;
    }
    if (state.net_session.lockstep_next_frame_to_step != 0) {
        if (status_out != nullptr) {
            *status_out = "Network run has already started.";
        }
        return false;
    }

    const std::uint32_t sequence = state.net_session.run_restart_next_sequence++;
    StorePendingRestart(
        state,
        sequence,
        0,
        stage_seed == 0 ? 1U : stage_seed,
        "classic",
        "classic_mines_1"
    );
    BroadcastRunRestart(state, *state.net_transport);
    if (status_out != nullptr) {
        *status_out = "Network run start scheduled.";
    }
    return true;
}

void SendPendingRunRestart(State& state, NetTransportRuntime& transport) {
    BroadcastRunRestart(state, transport);
    if (state.net_session.run_restart_applied_locally &&
        state.net_session.lockstep_next_frame_to_step >
            state.net_session.run_restart_apply_frame) {
        state.net_session.run_restart_pending = false;
    }
}

void HandleRunRestartPacket(State& state, const RunRestartPacket& packet) {
    state.net_session.run_restart_packets_received += 1;
    state.net_session.run_restart_last_packet_stage_instance_id = packet.stage_instance_id;
    state.net_session.run_restart_last_packet_sender_peer_id = packet.sender_peer_id;
    state.net_session.run_restart_last_packet_sequence = packet.sequence;
    state.net_session.run_restart_last_packet_stage_seed = packet.stage_seed;
    state.net_session.run_restart_last_ignore_reason.clear();

    const bool packet_matches_current_stage =
        packet.stage_instance_id == state.net_session.stage_instance_id;
    const bool packet_matches_next_stage =
        packet.stage_instance_id == state.net_session.stage_instance_id + 1;
    if (state.net_session.role != NetRole::Peer) {
        state.net_session.run_restart_packets_ignored += 1;
        state.net_session.run_restart_last_ignore_reason = "not-peer";
        return;
    }
    if (!packet_matches_current_stage && !packet_matches_next_stage) {
        state.net_session.run_restart_packets_ignored += 1;
        state.net_session.run_restart_last_ignore_reason = "stage";
        return;
    }
    if (packet.sender_peer_id != state.net_session.host_player_id) {
        state.net_session.run_restart_packets_ignored += 1;
        state.net_session.run_restart_last_ignore_reason = "sender";
        return;
    }
    if (packet.sequence < state.net_session.run_restart_last_sequence ||
        (packet.sequence == state.net_session.run_restart_last_sequence &&
         state.net_session.run_restart_applied_locally)) {
        state.net_session.run_restart_packets_ignored += 1;
        state.net_session.run_restart_last_ignore_reason = "sequence";
        return;
    }
    if (packet.sequence == state.net_session.run_restart_last_sequence &&
        state.net_session.run_restart_pending) {
        state.net_session.run_restart_packets_accepted += 1;
        return;
    }

    const LockstepFrame minimum_apply_frame =
        packet.apply_frame == 0 ? 0 : state.net_session.lockstep_next_frame_to_step + 1;
    StorePendingRestart(
        state,
        packet.sequence,
        std::max<LockstepFrame>(packet.apply_frame, minimum_apply_frame),
        packet.stage_seed,
        ReadFixedString(packet.quest_id),
        ReadFixedString(packet.quest_stage_id)
    );
    state.net_session.run_restart_packets_accepted += 1;
}

bool ApplyDueRunRestart(State& state) {
    if (!state.net_session.run_restart_pending ||
        state.net_session.run_restart_transition_queued ||
        state.net_session.run_restart_applied_locally ||
        state.net_session.lockstep_next_frame_to_step < state.net_session.run_restart_apply_frame) {
        return false;
    }

    QueueStageTransition(
        state,
        StageTransitionTarget{
            .destination = StageLoadTarget::ForQuestStage(
                state.net_session.run_restart_quest_id,
                state.net_session.run_restart_quest_stage_id
            ),
            .preserve_player_state = false,
            .seed = state.net_session.run_restart_stage_seed,
        }
    );
    state.net_session.run_restart_transition_queued = true;
    state.scene_frame = 0;
    state.game_over = false;
    state.pause = false;
    state.SetMode(Mode::StageTransition);
    return true;
}

void NotifyRunRestartStageLoaded(State& state) {
    if (!state.net_session.run_restart_pending ||
        !state.net_session.run_restart_transition_queued ||
        state.net_session.run_restart_applied_locally) {
        return;
    }
    if (state.stage.quest_id != state.net_session.run_restart_quest_id ||
        state.stage.quest_stage_id != state.net_session.run_restart_quest_stage_id ||
        state.stage.generation_seed.value_or(0) != state.net_session.run_restart_stage_seed) {
        return;
    }

    state.net_session.run_restart_transition_queued = false;
    state.net_session.run_restart_applied_locally = true;
    if (state.net_session.role != NetRole::Host) {
        state.net_session.run_restart_pending = false;
    }
}

} // namespace splonks::network

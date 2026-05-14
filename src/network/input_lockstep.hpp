#pragma once

#include "inputs.hpp"
#include "player_id.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace splonks::network {

using LockstepFrame = std::uint64_t;
using LockstepPeerId = std::uint32_t;
using LockstepSessionId = std::uint32_t;
using LockstepStageInstanceId = std::uint32_t;

struct LockstepInputRecord {
    PlayerId player_id = kInvalidPlayerId;
    LockstepFrame frame = 0;
    std::uint32_t sequence = 0;
    InputFrame input = InputFrame::New();
    bool predicted = false;
    bool canonical = false;
};

struct LockstepInputStoreResult {
    bool inserted = false;
    bool replaced_prediction = false;
    bool changed_existing = false;
    std::optional<LockstepFrame> mismatch_frame = std::nullopt;
};

struct LockstepInputPacket {
    LockstepSessionId session_id = 0;
    LockstepStageInstanceId stage_instance_id = 0;
    LockstepPeerId sender_peer_id = 0;
    std::uint32_t sequence = 0;
    std::vector<LockstepInputRecord> records;
};

struct LockstepHashPacket {
    LockstepSessionId session_id = 0;
    LockstepStageInstanceId stage_instance_id = 0;
    LockstepPeerId sender_peer_id = 0;
    LockstepFrame frame = 0;
    std::uint64_t gameplay_hash = 0;
};

std::uint32_t PackInputFrame(const InputFrame& input);
InputFrame UnpackInputFrame(std::uint32_t flags, UVec2 mouse_pos);

class LockstepInputBuffer {
public:
    LockstepInputStoreResult Store(const LockstepInputRecord& record);
    bool Has(PlayerId player_id, LockstepFrame frame) const;
    const InputFrame* Find(PlayerId player_id, LockstepFrame frame) const;
    const LockstepInputRecord* FindRecord(PlayerId player_id, LockstepFrame frame) const;
    const LockstepInputRecord* FindLatestRecordBefore(PlayerId player_id, LockstepFrame frame) const;
    std::optional<LockstepFrame> LatestFrameForPlayer(
        PlayerId player_id,
        bool include_predicted = true
    ) const;
    bool FrameReady(const std::vector<PlayerId>& required_players, LockstepFrame frame) const;
    bool BuildFrameInputs(
        const std::vector<PlayerId>& required_players,
        LockstepFrame frame,
        std::vector<InputFrame>& out_inputs
    ) const;
    void CollectRecords(
        const std::vector<PlayerId>& player_ids,
        LockstepFrame first_frame,
        LockstepFrame last_frame,
        std::vector<LockstepInputRecord>& out_records,
        std::size_t max_records
    ) const;
    void CollectCanonicalRecords(
        const std::vector<PlayerId>& player_ids,
        LockstepFrame first_frame,
        LockstepFrame last_frame,
        std::vector<LockstepInputRecord>& out_records,
        std::size_t max_records
    ) const;
    std::size_t RecordCount(bool include_predicted = true) const;
    std::size_t RecordCountForPlayer(PlayerId player_id, bool include_predicted = true) const;
    std::size_t PredictedRecordCount() const;
    bool HasPredictedRecordThroughFrame(LockstepFrame frame) const;
    void ClearBefore(LockstepFrame frame);

private:
    std::vector<LockstepInputRecord> records_;
};

} // namespace splonks::network

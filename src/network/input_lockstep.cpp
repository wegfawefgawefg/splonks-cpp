#include "network/input_lockstep.hpp"

#include <algorithm>

namespace splonks::network {

void LockstepInputBuffer::Store(const LockstepInputRecord& record) {
    if (record.player_id == kInvalidPlayerId) {
        return;
    }

    for (LockstepInputRecord& existing : records_) {
        if (existing.player_id == record.player_id && existing.frame == record.frame) {
            if (record.sequence >= existing.sequence) {
                existing = record;
            }
            return;
        }
    }

    records_.push_back(record);
}

bool LockstepInputBuffer::Has(PlayerId player_id, LockstepFrame frame) const {
    return Find(player_id, frame) != nullptr;
}

const PlayerInputFrame* LockstepInputBuffer::Find(
    PlayerId player_id,
    LockstepFrame frame
) const {
    for (const LockstepInputRecord& record : records_) {
        if (record.player_id == player_id && record.frame == frame) {
            return &record.input;
        }
    }
    return nullptr;
}

bool LockstepInputBuffer::FrameReady(
    const std::vector<PlayerId>& required_players,
    LockstepFrame frame
) const {
    return std::all_of(
        required_players.begin(),
        required_players.end(),
        [&](PlayerId player_id) {
            return Has(player_id, frame);
        }
    );
}

bool LockstepInputBuffer::BuildFrameInputs(
    const std::vector<PlayerId>& required_players,
    LockstepFrame frame,
    std::vector<PlayerInputFrame>& out_inputs
) const {
    out_inputs.clear();
    out_inputs.reserve(required_players.size());
    for (PlayerId player_id : required_players) {
        const PlayerInputFrame* const input = Find(player_id, frame);
        if (input == nullptr) {
            out_inputs.clear();
            return false;
        }
        out_inputs.push_back(*input);
    }
    return true;
}

void LockstepInputBuffer::ClearBefore(LockstepFrame frame) {
    records_.erase(
        std::remove_if(
            records_.begin(),
            records_.end(),
            [frame](const LockstepInputRecord& record) {
                return record.frame < frame;
            }
        ),
        records_.end()
    );
}

} // namespace splonks::network

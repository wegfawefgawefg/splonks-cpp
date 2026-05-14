#include "network/input_lockstep.hpp"

#include <algorithm>

namespace splonks::network {

namespace {

constexpr std::uint32_t kInputLeft = 1U << 0U;
constexpr std::uint32_t kInputRight = 1U << 1U;
constexpr std::uint32_t kInputUp = 1U << 2U;
constexpr std::uint32_t kInputDown = 1U << 3U;
constexpr std::uint32_t kInputJump = 1U << 4U;
constexpr std::uint32_t kInputRun = 1U << 5U;
constexpr std::uint32_t kInputUse = 1U << 6U;
constexpr std::uint32_t kInputEquip = 1U << 7U;
constexpr std::uint32_t kInputPickupDrop = 1U << 8U;
constexpr std::uint32_t kInputStop = 1U << 9U;
constexpr std::uint32_t kInputBomb = 1U << 10U;
constexpr std::uint32_t kInputRope = 1U << 11U;
constexpr std::uint32_t kInputAttack = 1U << 12U;
constexpr std::uint32_t kInputBuy = 1U << 13U;
constexpr std::uint32_t kInputEmoteUp = 1U << 14U;
constexpr std::uint32_t kInputEmoteDown = 1U << 15U;
constexpr std::uint32_t kInputQuit = 1U << 16U;
constexpr std::uint32_t kInputToggleCollisionBoxes = 1U << 17U;
constexpr std::uint32_t kInputRegenerateLevel = 1U << 18U;

void SetFlag(std::uint32_t& flags, std::uint32_t flag, bool enabled) {
    if (enabled) {
        flags |= flag;
    }
}

bool HasFlag(std::uint32_t flags, std::uint32_t flag) {
    return (flags & flag) != 0U;
}

bool InputFramesEqual(const InputFrame& lhs, const InputFrame& rhs) {
    return PackInputFrame(lhs) == PackInputFrame(rhs) &&
           lhs.mouse_pos.x == rhs.mouse_pos.x &&
           lhs.mouse_pos.y == rhs.mouse_pos.y;
}

} // namespace

std::uint32_t PackInputFrame(const InputFrame& input) {
    std::uint32_t flags = 0;
    SetFlag(flags, kInputLeft, input.left);
    SetFlag(flags, kInputRight, input.right);
    SetFlag(flags, kInputUp, input.up);
    SetFlag(flags, kInputDown, input.down);
    SetFlag(flags, kInputJump, input.jump);
    SetFlag(flags, kInputRun, input.run);
    SetFlag(flags, kInputUse, input.use_button);
    SetFlag(flags, kInputEquip, input.equip_button);
    SetFlag(flags, kInputPickupDrop, input.pick_up_drop);
    SetFlag(flags, kInputStop, input.stop);
    SetFlag(flags, kInputBomb, input.bomb);
    SetFlag(flags, kInputRope, input.rope);
    SetFlag(flags, kInputAttack, input.attack);
    SetFlag(flags, kInputBuy, input.buy_button);
    SetFlag(flags, kInputEmoteUp, input.emote_up);
    SetFlag(flags, kInputEmoteDown, input.emote_down);
    SetFlag(flags, kInputQuit, input.quit);
    SetFlag(flags, kInputToggleCollisionBoxes, input.toggle_collision_boxes);
    SetFlag(flags, kInputRegenerateLevel, input.regenerate_level);
    return flags;
}

InputFrame UnpackInputFrame(std::uint32_t flags, UVec2 mouse_pos) {
    InputFrame input = InputFrame::New();
    input.left = HasFlag(flags, kInputLeft);
    input.right = HasFlag(flags, kInputRight);
    input.up = HasFlag(flags, kInputUp);
    input.down = HasFlag(flags, kInputDown);
    input.jump = HasFlag(flags, kInputJump);
    input.run = HasFlag(flags, kInputRun);
    input.use_button = HasFlag(flags, kInputUse);
    input.equip_button = HasFlag(flags, kInputEquip);
    input.pick_up_drop = HasFlag(flags, kInputPickupDrop);
    input.stop = HasFlag(flags, kInputStop);
    input.bomb = HasFlag(flags, kInputBomb);
    input.rope = HasFlag(flags, kInputRope);
    input.attack = HasFlag(flags, kInputAttack);
    input.buy_button = HasFlag(flags, kInputBuy);
    input.emote_up = HasFlag(flags, kInputEmoteUp);
    input.emote_down = HasFlag(flags, kInputEmoteDown);
    input.quit = HasFlag(flags, kInputQuit);
    input.toggle_collision_boxes = HasFlag(flags, kInputToggleCollisionBoxes);
    input.regenerate_level = HasFlag(flags, kInputRegenerateLevel);
    input.mouse_pos = mouse_pos;
    return input;
}

LockstepInputStoreResult LockstepInputBuffer::Store(const LockstepInputRecord& record) {
    LockstepInputStoreResult result;
    if (record.player_id == kInvalidPlayerId) {
        return result;
    }

    for (LockstepInputRecord& existing : records_) {
        if (existing.player_id == record.player_id && existing.frame == record.frame) {
            if (existing.canonical && !record.canonical) {
                return result;
            }

            if (record.canonical) {
                if (!InputFramesEqual(existing.input, record.input)) {
                    result.changed_existing = true;
                    result.mismatch_frame = record.frame;
                }
                existing = record;
                return result;
            }

            if (existing.predicted && !record.predicted) {
                result.replaced_prediction = true;
                if (!InputFramesEqual(existing.input, record.input)) {
                    result.changed_existing = true;
                    result.mismatch_frame = record.frame;
                }
                existing = record;
                return result;
            }

            if (!existing.predicted && record.predicted) {
                return result;
            }

            if (record.sequence >= existing.sequence) {
                if (!InputFramesEqual(existing.input, record.input)) {
                    result.changed_existing = true;
                    result.mismatch_frame = record.frame;
                }
                existing = record;
            }
            return result;
        }
    }

    records_.push_back(record);
    result.inserted = true;
    return result;
}

bool LockstepInputBuffer::Has(PlayerId player_id, LockstepFrame frame) const {
    return Find(player_id, frame) != nullptr;
}

const InputFrame* LockstepInputBuffer::Find(
    PlayerId player_id,
    LockstepFrame frame
) const {
    const LockstepInputRecord* const record = FindRecord(player_id, frame);
    if (record == nullptr) {
        return nullptr;
    }
    return &record->input;
}

const LockstepInputRecord* LockstepInputBuffer::FindRecord(
    PlayerId player_id,
    LockstepFrame frame
) const {
    for (const LockstepInputRecord& record : records_) {
        if (record.player_id == player_id && record.frame == frame) {
            return &record;
        }
    }
    return nullptr;
}

const LockstepInputRecord* LockstepInputBuffer::FindLatestRecordBefore(
    PlayerId player_id,
    LockstepFrame frame
) const {
    const LockstepInputRecord* latest = nullptr;
    for (const LockstepInputRecord& record : records_) {
        if (record.player_id != player_id || record.frame >= frame || record.predicted) {
            continue;
        }
        if (latest == nullptr || record.frame > latest->frame) {
            latest = &record;
        }
    }
    return latest;
}

std::optional<LockstepFrame> LockstepInputBuffer::LatestFrameForPlayer(
    PlayerId player_id,
    bool include_predicted
) const {
    std::optional<LockstepFrame> latest;
    for (const LockstepInputRecord& record : records_) {
        if (record.player_id != player_id) {
            continue;
        }
        if (!include_predicted && record.predicted) {
            continue;
        }
        if (!latest.has_value() || record.frame > *latest) {
            latest = record.frame;
        }
    }
    return latest;
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
    std::vector<InputFrame>& out_inputs
) const {
    out_inputs.clear();
    out_inputs.reserve(required_players.size());
    for (PlayerId player_id : required_players) {
        const InputFrame* const input = Find(player_id, frame);
        if (input == nullptr) {
            out_inputs.clear();
            return false;
        }
        out_inputs.push_back(*input);
    }
    return true;
}

void LockstepInputBuffer::CollectRecords(
    const std::vector<PlayerId>& player_ids,
    LockstepFrame first_frame,
    LockstepFrame last_frame,
    std::vector<LockstepInputRecord>& out_records,
    std::size_t max_records
) const {
    for (LockstepFrame frame = first_frame; frame <= last_frame; ++frame) {
        for (PlayerId player_id : player_ids) {
            if (out_records.size() >= max_records) {
                return;
            }
            for (const LockstepInputRecord& record : records_) {
                if (record.player_id == player_id && record.frame == frame) {
                    out_records.push_back(record);
                    break;
                }
            }
        }
        if (frame == last_frame) {
            break;
        }
    }
}

void LockstepInputBuffer::CollectCanonicalRecords(
    const std::vector<PlayerId>& player_ids,
    LockstepFrame first_frame,
    LockstepFrame last_frame,
    std::vector<LockstepInputRecord>& out_records,
    std::size_t max_records
) const {
    for (LockstepFrame frame = first_frame; frame <= last_frame; ++frame) {
        for (PlayerId player_id : player_ids) {
            if (out_records.size() >= max_records) {
                return;
            }
            for (const LockstepInputRecord& record : records_) {
                if (record.player_id == player_id && record.frame == frame && record.canonical) {
                    out_records.push_back(record);
                    break;
                }
            }
        }
        if (frame == last_frame) {
            break;
        }
    }
}

std::size_t LockstepInputBuffer::RecordCount(bool include_predicted) const {
    if (include_predicted) {
        return records_.size();
    }
    return static_cast<std::size_t>(std::count_if(
        records_.begin(),
        records_.end(),
        [](const LockstepInputRecord& record) {
            return !record.predicted;
        }
    ));
}

std::size_t LockstepInputBuffer::RecordCountForPlayer(
    PlayerId player_id,
    bool include_predicted
) const {
    return static_cast<std::size_t>(std::count_if(
        records_.begin(),
        records_.end(),
        [player_id, include_predicted](const LockstepInputRecord& record) {
            return record.player_id == player_id && (include_predicted || !record.predicted);
        }
    ));
}

std::size_t LockstepInputBuffer::PredictedRecordCount() const {
    return static_cast<std::size_t>(std::count_if(
        records_.begin(),
        records_.end(),
        [](const LockstepInputRecord& record) {
            return record.predicted;
        }
    ));
}

bool LockstepInputBuffer::HasPredictedRecordThroughFrame(LockstepFrame frame) const {
    return std::any_of(
        records_.begin(),
        records_.end(),
        [frame](const LockstepInputRecord& record) {
            return record.predicted && record.frame <= frame;
        }
    );
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

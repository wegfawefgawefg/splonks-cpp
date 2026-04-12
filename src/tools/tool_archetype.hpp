#pragma once

#include "frame_data_id.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace splonks {

struct Audio;
struct Graphics;
struct State;
struct ToolSlot;

enum class ToolKind : std::uint8_t {
    ThrowPot,
    ThrowBomb,
    ThrowRope,
};

constexpr std::size_t ToolKindIndex(ToolKind kind) {
    return static_cast<std::size_t>(kind);
}

constexpr std::size_t kToolKindCount = ToolKindIndex(ToolKind::ThrowRope) + 1;

using ToolUseFn = bool (*) (
    std::size_t entity_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    std::size_t tool_slot_index,
    bool trigger_pressed
);

struct ToolArchetype {
    ToolKind kind = ToolKind::ThrowPot;
    const char* debug_name = "Unknown";
    FrameDataId icon_animation_id = kInvalidFrameDataId;
    std::uint16_t use_cooldown_frames = 0;
    std::optional<std::size_t> preferred_slot_index = std::nullopt;
    ToolUseFn use_fn = nullptr;
};

const ToolArchetype& GetToolArchetype(ToolKind kind);
const char* GetToolKindName(ToolKind kind);
std::optional<ToolKind> FindPreferredToolKindForSlotIndex(std::size_t slot_index);
void PopulateToolArchetypesTable();
void FillToolSlot(ToolSlot& slot, ToolKind kind, std::uint16_t count, bool active);

} // namespace splonks

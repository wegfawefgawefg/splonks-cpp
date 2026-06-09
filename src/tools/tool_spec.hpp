#pragma once

#include "aframe_id.hpp"
#include "math_types.hpp"
#include "sim/fxp.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace splonks {

namespace controls {
struct ControlIntent;
}

struct Audio;
struct Graphics;
struct State;
struct ToolSlot;

enum class ToolKind : std::uint8_t {
    ThrowPot,
    ThrowBomb,
    ThrowRope,
    ThrowStickyBomb,
};

constexpr std::size_t ToolKindIndex(ToolKind kind) {
    return static_cast<std::size_t>(kind);
}

constexpr std::size_t kToolKindCount = ToolKindIndex(ToolKind::ThrowStickyBomb) + 1;

using ToolThrowVelocityBuilder = sim::FxVec2 (*)(const controls::ControlIntent&);

using ToolUseFn = bool (*) (
    std::size_t ent_idx,
    State& state,
    Graphics& graphics,
    Audio& audio,
    std::size_t tool_slot_index,
    bool trigger_pressed,
    ToolThrowVelocityBuilder build_throw_velocity,
    std::optional<sim::FxVec2> throw_velocity_override
);

struct ToolSpec {
    ToolKind kind = ToolKind::ThrowPot;
    const char* debug_name = "Unknown";
    AFrameId icon_anim_id = kInvalidAFrameId;
    std::uint16_t use_cooldown_frames = 0;
    std::optional<std::uint32_t> preferred_slot_index = std::nullopt;
    ToolUseFn use_fn = nullptr;
};

const ToolSpec& GetToolSpec(ToolKind kind);
const char* GetToolKindName(ToolKind kind);
std::optional<ToolKind> FindPreferredToolKindForSlotIndex(std::size_t slot_index);
void PopulateToolSpecsTable();
void FillToolSlot(ToolSlot& slot, ToolKind kind, std::uint16_t count, bool active);

} // namespace splonks

#pragma once

#include "frame_data_id.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace splonks {

enum class HudEntrySource : std::uint8_t {
    Effect,
    HeldItem,
    BackItem,
    Temporary,
};

enum class HudEntryStyle : std::uint8_t {
    Normal,
    Dimmed,
    Flashing,
    Disabled,
};

enum class HudAnchor : std::uint8_t {
    TopLeft,
    Top,
    TopRight,
    Right,
    BottomLeft,
    Bottom,
    BottomRight,
    Left,
};

struct HudEntryKey {
    HudEntrySource source = HudEntrySource::Temporary;
    std::uint64_t id = 0;
};

struct HudBadge {
    bool active = false;
    HudAnchor anchor = HudAnchor::TopRight;
    FrameDataId icon_animation_id = kInvalidFrameDataId;
    std::optional<std::string> text;
    HudEntryStyle style = HudEntryStyle::Normal;
};

struct HudEntry {
    HudEntryKey key{};
    FrameDataId icon_animation_id = kInvalidFrameDataId;
    std::optional<std::string> count_text;
    HudAnchor count_anchor = HudAnchor::BottomRight;
    std::array<HudBadge, 4> badges{};
    HudEntryStyle style = HudEntryStyle::Normal;
    float shake = 0.0F;
    int hop_interval_frames = 0;
    float hop_amount = 2.0F;
    int extra_right_padding = 0;
};

} // namespace splonks

#pragma once

#include "vid.hpp"

#include <optional>

namespace splonks::entities::common {

enum class BlockingImpactAxis {
    Horizontal,
    Vertical,
};

enum class BlockingImpactSurface {
    StageBounds,
    Tiles,
    ImpassableEntity,
};

enum class ContactPhase {
    SweptEntered,
    AttemptedBlocked,
};

struct ContactContext {
    ContactPhase phase = ContactPhase::SweptEntered;
    bool has_impact = false;
    BlockingImpactAxis impact_axis = BlockingImpactAxis::Horizontal;
    BlockingImpactSurface impact_surface = BlockingImpactSurface::Tiles;
    float impact_velocity = 0.0F;
    int direction = 0;
    std::optional<VID> mover_vid = std::nullopt;
    std::optional<VID> other_vid = std::nullopt;
};

struct ContactResolution {
    bool blocks_movement = false;
    bool stop_sweep = false;
};

} // namespace splonks::entities::common

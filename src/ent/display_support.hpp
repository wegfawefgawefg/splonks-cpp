#pragma once

#include "entity/core_types.hpp"

namespace splonks {

struct EntityDisplayInput {
    EntityType type_ = EntityType::None;
    EntityDisplayState display_state = EntityDisplayState::Neutral;
};

} // namespace splonks

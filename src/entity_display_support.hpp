#pragma once

#include "entity_core_types.hpp"

namespace splonks {

struct EntityDisplayInput {
    EntityType type_ = EntityType::None;
    EntityDisplayState display_state = EntityDisplayState::Neutral;
};

} // namespace splonks

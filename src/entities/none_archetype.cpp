#include "entities/none_archetype.hpp"

namespace splonks {

const EntityArchetype kNoneArchetype{
    .type_ = EntityType::None,
    .has_physics = false,
    .can_collide = false,
};

} // namespace splonks

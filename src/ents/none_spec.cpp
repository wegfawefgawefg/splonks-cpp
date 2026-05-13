#include "entities/none_archetype.hpp"

#include "entity/core_types.hpp"

namespace splonks {

const EntityArchetype kNoneArchetype{
    .type_ = EntityType::None,
    .has_physics = false,
    .can_collide = false,
};

} // namespace splonks

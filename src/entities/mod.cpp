#include "entities/mod.hpp"

namespace splonks {

extern const EntityArchetype kNoneArchetype{
    .type_ = EntityType::None,
    .has_physics = false,
    .can_collide = false,
};

} // namespace splonks

namespace splonks::entities {

} // namespace splonks::entities

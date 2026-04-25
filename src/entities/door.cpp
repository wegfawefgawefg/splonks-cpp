#include "entities/door.hpp"

#include "entities/common/unimplemented_archetype.hpp"

namespace splonks::entities::door {

// TODO(classic): Door is not implemented yet. This archetype only keeps Classic Quest data spawnable.
extern const EntityArchetype kDoorArchetype =
    common::MakeUnimplementedClassicArchetype(EntityType::Door);

} // namespace splonks::entities::door

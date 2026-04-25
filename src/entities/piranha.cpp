#include "entities/piranha.hpp"

#include "entities/common/unimplemented_archetype.hpp"

namespace splonks::entities::piranha {

// TODO(classic): Piranha is not implemented yet. This archetype only keeps Classic Quest data spawnable.
extern const EntityArchetype kPiranhaArchetype =
    common::MakeUnimplementedClassicArchetype(EntityType::Piranha);

} // namespace splonks::entities::piranha

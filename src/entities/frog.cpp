#include "entities/frog.hpp"

#include "entities/common/unimplemented_archetype.hpp"

namespace splonks::entities::frog {

// TODO(classic): Frog entities are not implemented yet. These archetypes only keep Classic Quest data spawnable.
extern const EntityArchetype kFrogArchetype =
    common::MakeUnimplementedClassicArchetype(EntityType::Frog);
extern const EntityArchetype kFireFrogArchetype =
    common::MakeUnimplementedClassicArchetype(EntityType::FireFrog);

} // namespace splonks::entities::frog

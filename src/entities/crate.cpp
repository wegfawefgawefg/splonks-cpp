#include "entities/crate.hpp"

#include "entities/common/unimplemented_archetype.hpp"

namespace splonks::entities::crate {

// TODO(classic): Crate is not implemented yet. This archetype only keeps Classic Quest data spawnable.
extern const EntityArchetype kCrateArchetype =
    common::MakeUnimplementedClassicArchetype(EntityType::Crate);

} // namespace splonks::entities::crate

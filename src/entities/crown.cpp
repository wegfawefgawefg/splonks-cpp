#include "entities/crown.hpp"

#include "entities/common/unimplemented_archetype.hpp"

namespace splonks::entities::crown {

// TODO(classic): Crown is not implemented yet. This archetype only keeps Classic Quest data spawnable.
extern const EntityArchetype kCrownArchetype =
    common::MakeUnimplementedClassicArchetype(EntityType::Crown);

} // namespace splonks::entities::crown

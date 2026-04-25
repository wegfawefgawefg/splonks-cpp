#include "entities/mantrap.hpp"

#include "entities/common/unimplemented_archetype.hpp"

namespace splonks::entities::mantrap {

// TODO(classic): Mantrap is not implemented yet. This archetype only keeps Classic Quest data spawnable.
extern const EntityArchetype kMantrapArchetype =
    common::MakeUnimplementedClassicArchetype(EntityType::Mantrap);

} // namespace splonks::entities::mantrap

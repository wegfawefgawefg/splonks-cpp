#include "entities/ufo.hpp"

#include "entities/common/unimplemented_archetype.hpp"

namespace splonks::entities::ufo {

// TODO(classic): Ufo is not implemented yet. This archetype only keeps Classic Quest data spawnable.
extern const EntityArchetype kUfoArchetype =
    common::MakeUnimplementedClassicArchetype(EntityType::Ufo);

} // namespace splonks::entities::ufo

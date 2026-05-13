#include "entities/yeti.hpp"

#include "entities/common/unimplemented_archetype.hpp"

namespace splonks::entities::yeti {

// TODO(classic): Yeti is not implemented yet. This archetype only keeps Classic Quest data spawnable.
extern const EntityArchetype kYetiArchetype =
    common::MakeUnimplementedClassicArchetype(EntityType::Yeti);

} // namespace splonks::entities::yeti

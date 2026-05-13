#include "entities/jaws.hpp"

#include "entities/common/unimplemented_archetype.hpp"

namespace splonks::entities::jaws {

// TODO(classic): Jaws is not implemented yet. This archetype only keeps Classic Quest data spawnable.
extern const EntityArchetype kJawsArchetype =
    common::MakeUnimplementedClassicArchetype(EntityType::Jaws);

} // namespace splonks::entities::jaws

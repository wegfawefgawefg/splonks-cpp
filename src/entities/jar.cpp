#include "entities/jar.hpp"

#include "entities/common/unimplemented_archetype.hpp"

namespace splonks::entities::jar {

// TODO(classic): Jar is not implemented yet. This archetype only keeps Classic Quest data spawnable.
extern const EntityArchetype kJarArchetype =
    common::MakeUnimplementedClassicArchetype(EntityType::Jar);

} // namespace splonks::entities::jar

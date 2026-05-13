#include "entities/hawkman.hpp"

#include "entities/common/unimplemented_archetype.hpp"

namespace splonks::entities::hawkman {

// TODO(classic): Hawkman is not implemented yet. This archetype only keeps Classic Quest data spawnable.
extern const EntityArchetype kHawkmanArchetype =
    common::MakeUnimplementedClassicArchetype(EntityType::Hawkman);

} // namespace splonks::entities::hawkman

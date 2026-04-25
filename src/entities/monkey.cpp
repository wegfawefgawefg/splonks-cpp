#include "entities/monkey.hpp"

#include "entities/common/unimplemented_archetype.hpp"

namespace splonks::entities::monkey {

// TODO(classic): Monkey is not implemented yet. This archetype only keeps Classic Quest data spawnable.
extern const EntityArchetype kMonkeyArchetype =
    common::MakeUnimplementedClassicArchetype(EntityType::Monkey);

} // namespace splonks::entities::monkey

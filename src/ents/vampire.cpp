#include "entities/vampire.hpp"

#include "entities/common/unimplemented_archetype.hpp"

namespace splonks::entities::vampire {

// TODO(classic): Vampire is not implemented yet. This archetype only keeps Classic Quest data spawnable.
extern const EntityArchetype kVampireArchetype =
    common::MakeUnimplementedClassicArchetype(EntityType::Vampire);

} // namespace splonks::entities::vampire

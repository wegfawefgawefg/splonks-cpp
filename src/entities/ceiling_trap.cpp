#include "entities/ceiling_trap.hpp"

#include "entities/common/unimplemented_archetype.hpp"

namespace splonks::entities::ceiling_trap {

// TODO(classic): CeilingTrap is not implemented yet. This archetype only keeps Classic Quest data spawnable.
extern const EntityArchetype kCeilingTrapArchetype =
    common::MakeUnimplementedClassicArchetype(EntityType::CeilingTrap);

} // namespace splonks::entities::ceiling_trap

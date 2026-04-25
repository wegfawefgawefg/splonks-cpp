#include "entities/thwomp_trap.hpp"

#include "entities/common/unimplemented_archetype.hpp"

namespace splonks::entities::thwomp_trap {

// TODO(classic): ThwompTrap is not implemented yet. This archetype only keeps Classic Quest data spawnable.
extern const EntityArchetype kThwompTrapArchetype =
    common::MakeUnimplementedClassicArchetype(EntityType::ThwompTrap);

} // namespace splonks::entities::thwomp_trap

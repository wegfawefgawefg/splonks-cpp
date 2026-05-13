#include "entities/zombie.hpp"

#include "entities/common/unimplemented_archetype.hpp"

namespace splonks::entities::zombie {

// TODO(classic): Zombie is not implemented yet. This archetype only keeps Classic Quest data spawnable.
extern const EntityArchetype kZombieArchetype =
    common::MakeUnimplementedClassicArchetype(EntityType::Zombie);

} // namespace splonks::entities::zombie

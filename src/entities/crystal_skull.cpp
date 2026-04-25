#include "entities/crystal_skull.hpp"

#include "entities/common/unimplemented_archetype.hpp"

namespace splonks::entities::crystal_skull {

// TODO(classic): CrystalSkull is not implemented yet. This archetype only keeps Classic Quest data spawnable.
extern const EntityArchetype kCrystalSkullArchetype =
    common::MakeUnimplementedClassicArchetype(EntityType::CrystalSkull);

} // namespace splonks::entities::crystal_skull

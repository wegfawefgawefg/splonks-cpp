#include "entities/tomb_lord.hpp"

#include "entities/common/unimplemented_archetype.hpp"

namespace splonks::entities::tomb_lord {

// TODO(classic): TombLord is not implemented yet. This archetype only keeps Classic Quest data spawnable.
extern const EntityArchetype kTombLordArchetype =
    common::MakeUnimplementedClassicArchetype(EntityType::TombLord);

} // namespace splonks::entities::tomb_lord

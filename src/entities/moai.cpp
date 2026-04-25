#include "entities/moai.hpp"

#include "entities/common/unimplemented_archetype.hpp"

namespace splonks::entities::moai {

// TODO(classic): Moai entities are not implemented yet. These archetypes only keep Classic Quest data spawnable.
extern const EntityArchetype kMoaiArchetype =
    common::MakeUnimplementedClassicArchetype(EntityType::Moai);
extern const EntityArchetype kMoai2Archetype =
    common::MakeUnimplementedClassicArchetype(EntityType::Moai2);
extern const EntityArchetype kMoai3Archetype =
    common::MakeUnimplementedClassicArchetype(EntityType::Moai3);
extern const EntityArchetype kMoaiInsideArchetype =
    common::MakeUnimplementedClassicArchetype(EntityType::MoaiInside);

} // namespace splonks::entities::moai

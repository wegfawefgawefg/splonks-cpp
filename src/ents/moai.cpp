#include "entities/moai.hpp"

#include "entities/common/unimplemented_archetype.hpp"

namespace splonks::entities::moai {

// TODO(classic): Moai entities are not implemented yet. These archetypes only keep Classic Quest data spawnable.
extern const EntityArchetype kMoaiArchetype =
    common::MakeUnimplementedClassicNonStompableArchetype(EntityType::Moai);
extern const EntityArchetype kMoai2Archetype =
    common::MakeUnimplementedClassicNonStompableArchetype(EntityType::Moai2);
extern const EntityArchetype kMoai3Archetype =
    common::MakeUnimplementedClassicNonStompableArchetype(EntityType::Moai3);
extern const EntityArchetype kMoaiInsideArchetype =
    common::MakeUnimplementedClassicNonStompableArchetype(EntityType::MoaiInside);

} // namespace splonks::entities::moai

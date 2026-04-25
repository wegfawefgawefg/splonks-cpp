#include "entities/bones.hpp"

#include "entities/common/unimplemented_archetype.hpp"

namespace splonks::entities::bones {

// TODO(classic): Bones is not implemented yet. This archetype only keeps Classic Quest data spawnable.
extern const EntityArchetype kBonesArchetype =
    common::MakeUnimplementedClassicArchetype(EntityType::Bones);

} // namespace splonks::entities::bones

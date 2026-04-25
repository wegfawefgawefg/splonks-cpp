#include "entities/lamp.hpp"

#include "entities/common/unimplemented_archetype.hpp"

namespace splonks::entities::lamp {

// TODO(classic): Lamp entities are not implemented yet. These archetypes only keep Classic Quest data spawnable.
extern const EntityArchetype kLampArchetype =
    common::MakeUnimplementedClassicArchetype(EntityType::Lamp);
extern const EntityArchetype kLampRedArchetype =
    common::MakeUnimplementedClassicArchetype(EntityType::LampRed);

} // namespace splonks::entities::lamp

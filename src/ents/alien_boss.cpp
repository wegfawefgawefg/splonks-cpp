#include "entities/alien_boss.hpp"

#include "entities/common/unimplemented_archetype.hpp"

namespace splonks::entities::alien_boss {

// TODO(classic): AlienBoss is not implemented yet. This archetype only keeps Classic Quest data spawnable.
extern const EntityArchetype kAlienBossArchetype =
    common::MakeUnimplementedClassicArchetype(EntityType::AlienBoss);

} // namespace splonks::entities::alien_boss

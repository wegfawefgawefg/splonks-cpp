#include "entities/alien_ship.hpp"

#include "entities/common/unimplemented_archetype.hpp"

namespace splonks::entities::alien_ship {

// TODO(classic): AlienShip is not implemented yet. This archetype only keeps Classic Quest data spawnable.
extern const EntityArchetype kAlienShipArchetype =
    common::MakeUnimplementedClassicArchetype(EntityType::AlienShip);

} // namespace splonks::entities::alien_ship

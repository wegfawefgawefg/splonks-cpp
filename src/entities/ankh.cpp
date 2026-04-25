#include "entities/ankh.hpp"

#include "entities/common/unimplemented_archetype.hpp"

namespace splonks::entities::ankh {

// TODO(classic): Ankh is not implemented yet. This archetype only keeps Classic Quest data spawnable.
extern const EntityArchetype kAnkhArchetype =
    common::MakeUnimplementedClassicArchetype(EntityType::Ankh);

} // namespace splonks::entities::ankh

#include "entities/trap_block.hpp"

#include "entities/common/unimplemented_archetype.hpp"

namespace splonks::entities::trap_block {

// TODO(classic): TrapBlock is not implemented yet. This archetype only keeps Classic Quest data spawnable.
extern const EntityArchetype kTrapBlockArchetype =
    common::MakeUnimplementedClassicArchetype(EntityType::TrapBlock);

} // namespace splonks::entities::trap_block

#include "entities/xoc_block.hpp"

#include "entities/common/unimplemented_archetype.hpp"

namespace splonks::entities::xoc_block {

// TODO(classic): XocBlock is not implemented yet. This archetype only keeps Classic Quest data spawnable.
extern const EntityArchetype kXocBlockArchetype =
    common::MakeUnimplementedClassicNonStompableArchetype(EntityType::XocBlock);

} // namespace splonks::entities::xoc_block

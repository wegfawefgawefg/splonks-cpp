#include "entities/push_block.hpp"

#include "entities/common/unimplemented_archetype.hpp"

namespace splonks::entities::push_block {

// TODO(classic): PushBlock is not implemented yet. This archetype only keeps Classic Quest data spawnable.
extern const EntityArchetype kPushBlockArchetype =
    common::MakeUnimplementedClassicArchetype(EntityType::PushBlock);

} // namespace splonks::entities::push_block

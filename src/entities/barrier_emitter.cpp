#include "entities/barrier_emitter.hpp"

#include "entities/common/unimplemented_archetype.hpp"

namespace splonks::entities::barrier_emitter {

// TODO(classic): BarrierEmitter is not implemented yet. This archetype only keeps Classic Quest data spawnable.
extern const EntityArchetype kBarrierEmitterArchetype =
    common::MakeUnimplementedClassicArchetype(EntityType::BarrierEmitter);

} // namespace splonks::entities::barrier_emitter

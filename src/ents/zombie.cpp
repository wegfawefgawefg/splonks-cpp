#include "ents/zombie.hpp"

#include "ents/common/unimplemented_spec.hpp"

namespace splonks::ents::zombie {

// TODO(classic): Zombie is not implemented yet. This spec only keeps Classic Quest data spawnable.
extern const EntSpec kZombieSpec =
    common::MakeUnimplementedClassicSpec(EntType::Zombie);

} // namespace splonks::ents::zombie

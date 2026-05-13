#include "ents/ceiling_trap.hpp"

#include "ents/common/unimplemented_spec.hpp"

namespace splonks::ents::ceiling_trap {

// TODO(classic): CeilingTrap is not implemented yet. This spec only keeps Classic Quest data spawnable.
extern const EntSpec kCeilingTrapSpec =
    common::MakeUnimplementedClassicNonStompableSpec(EntType::CeilingTrap);

} // namespace splonks::ents::ceiling_trap

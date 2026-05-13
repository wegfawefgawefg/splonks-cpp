#include "ents/crown.hpp"

#include "ents/common/unimplemented_spec.hpp"

namespace splonks::ents::crown {

// TODO(classic): Crown is not implemented yet. This spec only keeps Classic Quest data spawnable.
extern const EntSpec kCrownSpec =
    common::MakeUnimplementedClassicNonStompableSpec(EntType::Crown);

} // namespace splonks::ents::crown

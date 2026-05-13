#include "ents/xoc_block.hpp"

#include "ents/common/unimplemented_spec.hpp"

namespace splonks::ents::xoc_block {

// TODO(classic): XocBlock is not implemented yet. This spec only keeps Classic Quest data spawnable.
extern const EntSpec kXocBlockSpec =
    common::MakeUnimplementedClassicNonStompableSpec(EntType::XocBlock);

} // namespace splonks::ents::xoc_block

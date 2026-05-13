#include "ents/frog.hpp"

#include "ents/common/unimplemented_spec.hpp"

namespace splonks::ents::frog {

// TODO(classic): Frog ents are not implemented yet. These specs only keep Classic Quest data spawnable.
extern const EntSpec kFrogSpec =
    common::MakeUnimplementedClassicSpec(EntType::Frog);
extern const EntSpec kFireFrogSpec =
    common::MakeUnimplementedClassicSpec(EntType::FireFrog);

} // namespace splonks::ents::frog

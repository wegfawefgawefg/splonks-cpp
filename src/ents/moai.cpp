#include "ents/moai.hpp"

#include "ents/common/unimplemented_spec.hpp"

namespace splonks::ents::moai {

// TODO(classic): Moai ents are not implemented yet. These specs only keep Classic Quest data spawnable.
extern const EntSpec kMoaiSpec =
    common::MakeUnimplementedClassicNonStompableSpec(EntType::Moai);
extern const EntSpec kMoai2Spec =
    common::MakeUnimplementedClassicNonStompableSpec(EntType::Moai2);
extern const EntSpec kMoai3Spec =
    common::MakeUnimplementedClassicNonStompableSpec(EntType::Moai3);
extern const EntSpec kMoaiInsideSpec =
    common::MakeUnimplementedClassicNonStompableSpec(EntType::MoaiInside);

} // namespace splonks::ents::moai

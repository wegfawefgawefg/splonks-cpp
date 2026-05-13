#include "ents/tomb_lord.hpp"

#include "ents/common/unimplemented_spec.hpp"

namespace splonks::ents::tomb_lord {

// TODO(classic): TombLord is not implemented yet. This spec only keeps Classic Quest data spawnable.
extern const EntSpec kTombLordSpec =
    common::MakeUnimplementedClassicSpec(EntType::TombLord);

} // namespace splonks::ents::tomb_lord

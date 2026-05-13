#include "ents/none_spec.hpp"

#include "ent/core_types.hpp"

namespace splonks {

const EntSpec kNoneSpec{
    .type_ = EntType::None,
    .has_physics = false,
    .can_collide = false,
};

} // namespace splonks

#include "gameplay_authority.hpp"

namespace splonks {

bool HasLocalGameplayAuthorityForEntity(const State& state, VID entity_vid) {
    (void)state;
    (void)entity_vid;
    return true;
}

bool HasLocalGameplayAuthorityForInteractionSource(const State& state, VID entity_vid) {
    (void)state;
    (void)entity_vid;
    return true;
}

} // namespace splonks

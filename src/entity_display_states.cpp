#include "entity_display_states.hpp"

namespace splonks {

Sprite GetSpriteForDisplayState(const EntityDisplayInput& entity) {
    switch (entity.type_) {
    case EntityType::Bat:
        switch (entity.display_state) {
        case EntityDisplayState::Neutral:
            return Sprite::BatNeutral;
        case EntityDisplayState::Dead:
            return Sprite::BatDead;
        case EntityDisplayState::Hanging:
            return Sprite::BatHanging;
        default:
            return Sprite::NoSprite;
        }
    default:
        return Sprite::NoSprite;
    }
}

} // namespace splonks

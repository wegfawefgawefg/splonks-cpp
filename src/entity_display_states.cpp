#include "entity_display_states.hpp"

namespace splonks {

std::optional<DisplayStateFrameDataSelection> GetFrameDataSelectionForDisplayState(
    const EntityDisplayInput& entity
) {
    switch (entity.type_) {
    case EntityType::Player:
        switch (entity.display_state) {
        case EntityDisplayState::Neutral:
            return DisplayStateFrameDataSelection{frame_data_ids::PlayerStanding, true, false, 0};
        case EntityDisplayState::NeutralHolding:
            return DisplayStateFrameDataSelection{
                frame_data_ids::PlayerStandingHolding, true, false, 0};
        case EntityDisplayState::Walk:
            return DisplayStateFrameDataSelection{frame_data_ids::PlayerWalking, true, false, 0};
        case EntityDisplayState::WalkHolding:
            return DisplayStateFrameDataSelection{
                frame_data_ids::PlayerWalkHolding, true, false, 0};
        case EntityDisplayState::Fly:
            return DisplayStateFrameDataSelection{frame_data_ids::PlayerFalling, true, false, 0};
        case EntityDisplayState::Dead:
            return DisplayStateFrameDataSelection{frame_data_ids::PlayerDead, true, false, 0};
        case EntityDisplayState::Stunned:
            return DisplayStateFrameDataSelection{frame_data_ids::PlayerStunned, true, false, 0};
        case EntityDisplayState::Climbing:
            return DisplayStateFrameDataSelection{frame_data_ids::PlayerClimbing, true, false, 0};
        case EntityDisplayState::Hanging:
            return DisplayStateFrameDataSelection{frame_data_ids::PlayerHanging, true, false, 0};
        case EntityDisplayState::Falling:
            return DisplayStateFrameDataSelection{frame_data_ids::PlayerFalling, true, false, 0};
        }
        break;
    case EntityType::Bat:
        switch (entity.display_state) {
        case EntityDisplayState::Neutral:
        case EntityDisplayState::Hanging:
            return DisplayStateFrameDataSelection{frame_data_ids::HangingBat, true, false, 0};
        case EntityDisplayState::Fly:
            return DisplayStateFrameDataSelection{frame_data_ids::FlyingBat, true, false, 0};
        case EntityDisplayState::Dead:
            return DisplayStateFrameDataSelection{frame_data_ids::DeadBat, true, false, 0};
        default:
            return std::nullopt;
        }
    case EntityType::BaseballBat:
        switch (entity.display_state) {
        case EntityDisplayState::Neutral:
        case EntityDisplayState::NeutralHolding:
        case EntityDisplayState::Walk:
            return DisplayStateFrameDataSelection{
                frame_data_ids::BaseballBatSwing, true, false, 0};
        default:
            return std::nullopt;
        }
    default:
        return std::nullopt;
    }

    return std::nullopt;
}

} // namespace splonks

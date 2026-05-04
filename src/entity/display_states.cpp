#include "entity/display_states.hpp"

#include "entity/core_types.hpp"

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
        case EntityDisplayState::EmoteDab:
            return DisplayStateFrameDataSelection{frame_data_ids::PlayerDab, true, false, 0};
        case EntityDisplayState::EmoteBald:
            return DisplayStateFrameDataSelection{frame_data_ids::PlayerBald, true, false, 0};
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
    case EntityType::Snake:
        switch (entity.display_state) {
        case EntityDisplayState::Neutral:
        case EntityDisplayState::Stunned:
            return DisplayStateFrameDataSelection{frame_data_ids::Snake, true, false, 0};
        case EntityDisplayState::Walk:
            return DisplayStateFrameDataSelection{frame_data_ids::SnakeWalk, true, false, 0};
        case EntityDisplayState::Dead:
            return DisplayStateFrameDataSelection{frame_data_ids::SnakeDead, true, false, 0};
        default:
            return std::nullopt;
        }
    case EntityType::Cobra:
        switch (entity.display_state) {
        case EntityDisplayState::Neutral:
        case EntityDisplayState::Stunned:
            return DisplayStateFrameDataSelection{frame_data_ids::Cobra, true, false, 0};
        case EntityDisplayState::Walk:
            return DisplayStateFrameDataSelection{frame_data_ids::CobraWalk, true, false, 0};
        case EntityDisplayState::Dead:
            return DisplayStateFrameDataSelection{frame_data_ids::CobraDead, true, false, 0};
        default:
            return std::nullopt;
        }
    case EntityType::CobraSpit:
        switch (entity.display_state) {
        case EntityDisplayState::Neutral:
        case EntityDisplayState::Walk:
        case EntityDisplayState::Fly:
        case EntityDisplayState::Dead:
        case EntityDisplayState::Stunned:
        case EntityDisplayState::Falling:
            return DisplayStateFrameDataSelection{frame_data_ids::CobraSpit, true, false, 0};
        default:
            return std::nullopt;
        }
    case EntityType::Mantrap:
        switch (entity.display_state) {
        case EntityDisplayState::Neutral:
        case EntityDisplayState::Stunned:
        case EntityDisplayState::Dead:
            return DisplayStateFrameDataSelection{frame_data_ids::Mantrap, true, false, 0};
        case EntityDisplayState::Walk:
            return DisplayStateFrameDataSelection{frame_data_ids::MantrapWalk, true, false, 0};
        default:
            return std::nullopt;
        }
    case EntityType::Piranha:
        switch (entity.display_state) {
        case EntityDisplayState::Dead:
            return DisplayStateFrameDataSelection{frame_data_ids::PiranhaDead, true, false, 0};
        case EntityDisplayState::Neutral:
            return DisplayStateFrameDataSelection{frame_data_ids::Piranha, true, false, 0};
        case EntityDisplayState::Walk:
        case EntityDisplayState::Fly:
        case EntityDisplayState::Stunned:
        case EntityDisplayState::Falling:
            return DisplayStateFrameDataSelection{frame_data_ids::PiranhaSwim, true, false, 0};
        default:
            return std::nullopt;
        }
    case EntityType::Monkey:
        switch (entity.display_state) {
        case EntityDisplayState::Neutral:
            return DisplayStateFrameDataSelection{frame_data_ids::MonkeyStand, true, false, 0};
        case EntityDisplayState::Stunned:
        case EntityDisplayState::Dead:
            return DisplayStateFrameDataSelection{frame_data_ids::MonkeyDead, true, false, 0};
        case EntityDisplayState::Walk:
            return DisplayStateFrameDataSelection{frame_data_ids::MonkeyDown, true, false, 0};
        case EntityDisplayState::Fly:
        case EntityDisplayState::Falling:
            return DisplayStateFrameDataSelection{frame_data_ids::MonkeyHang, true, false, 0};
        case EntityDisplayState::Climbing:
        case EntityDisplayState::Hanging:
            return DisplayStateFrameDataSelection{frame_data_ids::MonkeyHang, true, false, 0};
        default:
            return std::nullopt;
        }
    case EntityType::Caveman:
        switch (entity.display_state) {
        case EntityDisplayState::Neutral:
            return DisplayStateFrameDataSelection{frame_data_ids::Caveman, true, false, 0};
        case EntityDisplayState::Walk:
            return DisplayStateFrameDataSelection{frame_data_ids::CavemanWalk, true, false, 0};
        case EntityDisplayState::Stunned:
            return DisplayStateFrameDataSelection{frame_data_ids::CavemanStunned, true, false, 0};
        case EntityDisplayState::Dead:
            return DisplayStateFrameDataSelection{frame_data_ids::CavemanDead, true, false, 0};
        default:
            return std::nullopt;
        }
    case EntityType::Skull:
        switch (entity.display_state) {
        case EntityDisplayState::Neutral:
        case EntityDisplayState::Walk:
        case EntityDisplayState::Dead:
        case EntityDisplayState::Stunned:
        case EntityDisplayState::Falling:
            return DisplayStateFrameDataSelection{frame_data_ids::Skull, true, false, 0};
        default:
            return std::nullopt;
        }
    case EntityType::Skeleton:
        switch (entity.display_state) {
        case EntityDisplayState::Neutral:
        case EntityDisplayState::Stunned:
        case EntityDisplayState::Dead:
            return DisplayStateFrameDataSelection{frame_data_ids::Skull, true, false, 0};
        case EntityDisplayState::Walk:
            return DisplayStateFrameDataSelection{frame_data_ids::SkeletonWalk, true, false, 0};
        default:
            return std::nullopt;
        }
    case EntityType::Damsel:
        switch (entity.display_state) {
        case EntityDisplayState::Neutral:
            return DisplayStateFrameDataSelection{frame_data_ids::Damsel, true, false, 0};
        case EntityDisplayState::Walk:
            return DisplayStateFrameDataSelection{frame_data_ids::DamselWalk, true, false, 0};
        case EntityDisplayState::Stunned:
            return DisplayStateFrameDataSelection{frame_data_ids::DamselStunned, true, false, 0};
        case EntityDisplayState::Dead:
            return DisplayStateFrameDataSelection{frame_data_ids::DamselDead, true, false, 0};
        default:
            return std::nullopt;
        }
    case EntityType::Spider:
        switch (entity.display_state) {
        case EntityDisplayState::Neutral:
        case EntityDisplayState::Walk:
        case EntityDisplayState::Stunned:
        case EntityDisplayState::Dead:
            return DisplayStateFrameDataSelection{frame_data_ids::Spider, true, false, 0};
        default:
            return std::nullopt;
        }
    case EntityType::RageSpider:
        switch (entity.display_state) {
        case EntityDisplayState::Neutral:
        case EntityDisplayState::Walk:
        case EntityDisplayState::Stunned:
        case EntityDisplayState::Dead:
            return DisplayStateFrameDataSelection{frame_data_ids::RageSpider, true, false, 0};
        default:
            return std::nullopt;
        }
    case EntityType::GiantSpider:
        switch (entity.display_state) {
        case EntityDisplayState::Neutral:
        case EntityDisplayState::Walk:
        case EntityDisplayState::Dead:
            return DisplayStateFrameDataSelection{frame_data_ids::GiantSpider, true, false, 0};
        default:
            return std::nullopt;
        }
    default:
        return std::nullopt;
    }

    return std::nullopt;
}

} // namespace splonks

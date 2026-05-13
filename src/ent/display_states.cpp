#include "ent/display_states.hpp"

#include "ent/core_types.hpp"

namespace splonks {

std::optional<DisplayStateAFrameSelection> GetAFrameSelectionForDisplayState(
    const EntDisplayInput& ent
) {
    switch (ent.type_) {
    case EntType::Player:
        switch (ent.display_state) {
        case EntDisplayState::Neutral:
            return DisplayStateAFrameSelection{aframe_ids::PlayerStanding, true, false, 0};
        case EntDisplayState::NeutralHolding:
            return DisplayStateAFrameSelection{
                aframe_ids::PlayerStandingHolding, true, false, 0};
        case EntDisplayState::Walk:
            return DisplayStateAFrameSelection{aframe_ids::PlayerWalking, true, false, 0};
        case EntDisplayState::WalkHolding:
            return DisplayStateAFrameSelection{
                aframe_ids::PlayerWalkHolding, true, false, 0};
        case EntDisplayState::Fly:
            return DisplayStateAFrameSelection{aframe_ids::PlayerFalling, true, false, 0};
        case EntDisplayState::Dead:
            return DisplayStateAFrameSelection{aframe_ids::PlayerDead, true, false, 0};
        case EntDisplayState::Stunned:
            return DisplayStateAFrameSelection{aframe_ids::PlayerStunned, true, false, 0};
        case EntDisplayState::Climbing:
            return DisplayStateAFrameSelection{aframe_ids::PlayerClimbing, true, false, 0};
        case EntDisplayState::Hanging:
            return DisplayStateAFrameSelection{aframe_ids::PlayerHanging, true, false, 0};
        case EntDisplayState::Falling:
            return DisplayStateAFrameSelection{aframe_ids::PlayerFalling, true, false, 0};
        case EntDisplayState::EmoteDab:
            return DisplayStateAFrameSelection{aframe_ids::PlayerDab, true, false, 0};
        case EntDisplayState::EmoteBald:
            return DisplayStateAFrameSelection{aframe_ids::PlayerBald, true, false, 0};
        }
        break;
    case EntType::Bat:
        switch (ent.display_state) {
        case EntDisplayState::Neutral:
        case EntDisplayState::Hanging:
            return DisplayStateAFrameSelection{aframe_ids::HangingBat, true, false, 0};
        case EntDisplayState::Fly:
            return DisplayStateAFrameSelection{aframe_ids::FlyingBat, true, false, 0};
        case EntDisplayState::Dead:
            return DisplayStateAFrameSelection{aframe_ids::DeadBat, true, false, 0};
        default:
            return std::nullopt;
        }
    case EntType::BaseballBat:
        switch (ent.display_state) {
        case EntDisplayState::Neutral:
        case EntDisplayState::NeutralHolding:
        case EntDisplayState::Walk:
            return DisplayStateAFrameSelection{
                aframe_ids::BaseballBatSwing, true, false, 0};
        default:
            return std::nullopt;
        }
    case EntType::Snake:
        switch (ent.display_state) {
        case EntDisplayState::Neutral:
        case EntDisplayState::Stunned:
            return DisplayStateAFrameSelection{aframe_ids::Snake, true, false, 0};
        case EntDisplayState::Walk:
            return DisplayStateAFrameSelection{aframe_ids::SnakeWalk, true, false, 0};
        case EntDisplayState::Dead:
            return DisplayStateAFrameSelection{aframe_ids::SnakeDead, true, false, 0};
        default:
            return std::nullopt;
        }
    case EntType::Cobra:
        switch (ent.display_state) {
        case EntDisplayState::Neutral:
        case EntDisplayState::Stunned:
            return DisplayStateAFrameSelection{aframe_ids::Cobra, true, false, 0};
        case EntDisplayState::Walk:
            return DisplayStateAFrameSelection{aframe_ids::CobraWalk, true, false, 0};
        case EntDisplayState::Dead:
            return DisplayStateAFrameSelection{aframe_ids::CobraDead, true, false, 0};
        default:
            return std::nullopt;
        }
    case EntType::CobraSpit:
        switch (ent.display_state) {
        case EntDisplayState::Neutral:
        case EntDisplayState::Walk:
        case EntDisplayState::Fly:
        case EntDisplayState::Dead:
        case EntDisplayState::Stunned:
        case EntDisplayState::Falling:
            return DisplayStateAFrameSelection{aframe_ids::CobraSpit, true, false, 0};
        default:
            return std::nullopt;
        }
    case EntType::Mantrap:
        switch (ent.display_state) {
        case EntDisplayState::Neutral:
        case EntDisplayState::Stunned:
        case EntDisplayState::Dead:
            return DisplayStateAFrameSelection{aframe_ids::Mantrap, true, false, 0};
        case EntDisplayState::Walk:
            return DisplayStateAFrameSelection{aframe_ids::MantrapWalk, true, false, 0};
        default:
            return std::nullopt;
        }
    case EntType::Piranha:
        switch (ent.display_state) {
        case EntDisplayState::Dead:
            return DisplayStateAFrameSelection{aframe_ids::PiranhaDead, true, false, 0};
        case EntDisplayState::Neutral:
            return DisplayStateAFrameSelection{aframe_ids::Piranha, true, false, 0};
        case EntDisplayState::Walk:
        case EntDisplayState::Fly:
        case EntDisplayState::Stunned:
        case EntDisplayState::Falling:
            return DisplayStateAFrameSelection{aframe_ids::PiranhaSwim, true, false, 0};
        default:
            return std::nullopt;
        }
    case EntType::Monkey:
        switch (ent.display_state) {
        case EntDisplayState::Neutral:
            return DisplayStateAFrameSelection{aframe_ids::MonkeyStand, true, false, 0};
        case EntDisplayState::Stunned:
        case EntDisplayState::Dead:
            return DisplayStateAFrameSelection{aframe_ids::MonkeyDead, true, false, 0};
        case EntDisplayState::Walk:
            return DisplayStateAFrameSelection{aframe_ids::MonkeyDown, true, false, 0};
        case EntDisplayState::Fly:
        case EntDisplayState::Falling:
            return DisplayStateAFrameSelection{aframe_ids::MonkeyHang, true, false, 0};
        case EntDisplayState::Climbing:
        case EntDisplayState::Hanging:
            return DisplayStateAFrameSelection{aframe_ids::MonkeyHang, true, false, 0};
        default:
            return std::nullopt;
        }
    case EntType::Caveman:
        switch (ent.display_state) {
        case EntDisplayState::Neutral:
            return DisplayStateAFrameSelection{aframe_ids::Caveman, true, false, 0};
        case EntDisplayState::Walk:
            return DisplayStateAFrameSelection{aframe_ids::CavemanWalk, true, false, 0};
        case EntDisplayState::Stunned:
            return DisplayStateAFrameSelection{aframe_ids::CavemanStunned, true, false, 0};
        case EntDisplayState::Dead:
            return DisplayStateAFrameSelection{aframe_ids::CavemanDead, true, false, 0};
        default:
            return std::nullopt;
        }
    case EntType::Skull:
        switch (ent.display_state) {
        case EntDisplayState::Neutral:
        case EntDisplayState::Walk:
        case EntDisplayState::Dead:
        case EntDisplayState::Stunned:
        case EntDisplayState::Falling:
            return DisplayStateAFrameSelection{aframe_ids::Skull, true, false, 0};
        default:
            return std::nullopt;
        }
    case EntType::Skeleton:
        switch (ent.display_state) {
        case EntDisplayState::Neutral:
        case EntDisplayState::Stunned:
        case EntDisplayState::Dead:
            return DisplayStateAFrameSelection{aframe_ids::Skull, true, false, 0};
        case EntDisplayState::Walk:
            return DisplayStateAFrameSelection{aframe_ids::SkeletonWalk, true, false, 0};
        default:
            return std::nullopt;
        }
    case EntType::Damsel:
        switch (ent.display_state) {
        case EntDisplayState::Neutral:
            return DisplayStateAFrameSelection{aframe_ids::Damsel, true, false, 0};
        case EntDisplayState::Walk:
            return DisplayStateAFrameSelection{aframe_ids::DamselWalk, true, false, 0};
        case EntDisplayState::Stunned:
            return DisplayStateAFrameSelection{aframe_ids::DamselStunned, true, false, 0};
        case EntDisplayState::Dead:
            return DisplayStateAFrameSelection{aframe_ids::DamselDead, true, false, 0};
        default:
            return std::nullopt;
        }
    case EntType::Spider:
        switch (ent.display_state) {
        case EntDisplayState::Neutral:
        case EntDisplayState::Walk:
        case EntDisplayState::Stunned:
        case EntDisplayState::Dead:
            return DisplayStateAFrameSelection{aframe_ids::Spider, true, false, 0};
        default:
            return std::nullopt;
        }
    case EntType::RageSpider:
        switch (ent.display_state) {
        case EntDisplayState::Neutral:
        case EntDisplayState::Walk:
        case EntDisplayState::Stunned:
        case EntDisplayState::Dead:
            return DisplayStateAFrameSelection{aframe_ids::RageSpider, true, false, 0};
        default:
            return std::nullopt;
        }
    case EntType::GiantSpider:
        switch (ent.display_state) {
        case EntDisplayState::Neutral:
        case EntDisplayState::Walk:
        case EntDisplayState::Dead:
            return DisplayStateAFrameSelection{aframe_ids::GiantSpider, true, false, 0};
        default:
            return std::nullopt;
        }
    default:
        return std::nullopt;
    }

    return std::nullopt;
}

} // namespace splonks
